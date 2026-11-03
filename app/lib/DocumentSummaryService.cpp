#include "DocumentSummaryService.hpp"
#include "ILLMClient.hpp"
#include "LLMErrors.hpp"

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#else
#error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace {
constexpr std::size_t kMaximumSummaryRequests = 64;
constexpr int kMaximumSummaryDepth = 8;
constexpr std::size_t kExtractiveSummaryWords = 120;
constexpr std::size_t kExtractiveSummaryBytes = 1200;
constexpr std::size_t kChunkSplitDivisor = 2;
// Prefer a word boundary in the final quarter of the token-fitted chunk.
constexpr std::size_t kWordBoundaryNumerator = 3;
constexpr std::size_t kWordBoundaryDenominator = 4;

struct ParsedAnalysis {
    std::string summary;
    std::string filename;
};

std::string trim(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::optional<ParsedAnalysis> parse_analysis_json(const std::string& response)
{
    Json::CharReaderBuilder reader;
    Json::Value root;
    std::string errors;
    const auto try_parse = [&](const std::string& payload) {
        std::istringstream stream(payload);
        return Json::parseFromStream(reader, stream, &root, &errors);
    };
    if (!try_parse(response)) {
        const auto first = response.find('{');
        const auto last = response.rfind('}');
        if (first == std::string::npos || last == std::string::npos || last <= first) return std::nullopt;
        if (!try_parse(response.substr(first, last - first + 1))) return std::nullopt;
    }
    if (!root.isObject()) return std::nullopt;
    ParsedAnalysis parsed;
    if (root.isMember("summary")) parsed.summary = root["summary"].asString();
    else if (root.isMember("description")) parsed.summary = root["description"].asString();
    if (root.isMember("filename")) parsed.filename = root["filename"].asString();
    else if (root.isMember("name")) parsed.filename = root["name"].asString();
    else if (root.isMember("suggested_name")) parsed.filename = root["suggested_name"].asString();
    return parsed;
}

std::size_t utf8_boundary(const std::string& text, std::size_t end)
{
    end = std::min(end, text.size());
    while (end > 0 && end < text.size() && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    return end;
}

std::string first_words(const std::string& text, std::size_t max_words)
{
    std::istringstream input(text);
    std::string word, result;
    std::size_t count = 0;
    while (input >> word) {
        if (!result.empty()) result += ' ';
        result += word;
        if (++count >= max_words) break;
    }
    return result;
}

struct Capacity {
    std::optional<ILLMClient::CompletionBudget> budget;
    std::optional<std::size_t> available;
};

std::optional<std::size_t> input_capacity(const std::optional<ILLMClient::CompletionBudget>& budget)
{
    if (!budget) return std::nullopt;
    return budget->maximum_context_tokens > budget->response_tokens
        ? budget->maximum_context_tokens - budget->response_tokens : 0;
}

/** @brief Own the bounded recursion and diagnostics for a single document. */
class SummaryRun {
public:
    SummaryRun(ILLMClient& llm, const DocumentSummaryService::PromptBuilder& build_prompt,
        int response_tokens, const std::function<bool()>& should_stop)
        : llm_(llm), build_prompt_(build_prompt), response_tokens_(response_tokens), should_stop_(should_stop) {}

    DocumentSummaryResult run(const std::string& evidence)
    {
        auto parsed = summarize(evidence, false, 0);
        result_.summary = std::move(parsed.summary);
        result_.filename = std::move(parsed.filename);
        return std::move(result_);
    }

private:
    void check_stopped() const
    {
        if (should_stop_ && should_stop_()) throw std::runtime_error("Document analysis cancelled.");
    }

    ParsedAnalysis extractive(const std::string& text)
    {
        result_.used_extractive_fallback = true;
        const auto words = trim(first_words(text, kExtractiveSummaryWords));
        return {words.substr(0, utf8_boundary(words, kExtractiveSummaryBytes)), {}};
    }

    void refresh_capacity(const std::string& prompt, const PromptBudgetError& error, Capacity& capacity) const
    {
        if (error.input_budget()) capacity.available = error.input_budget();
        // Native allocation may have revealed a lower memory ceiling.
        capacity.budget = llm_.completion_budget(prompt, response_tokens_);
        if (!capacity.available) capacity.available = input_capacity(capacity.budget);
    }

    std::optional<ParsedAnalysis> try_complete(const std::string& text, const std::string& prompt,
        bool merging, Capacity& capacity)
    {
        if (capacity.budget && capacity.budget->prompt_tokens > *capacity.available) return std::nullopt;
        try {
            ++result_.requests;
            const auto response = llm_.complete_document_prompt(prompt, response_tokens_);
            check_stopped();
            if (!merging) ++result_.source_chunks;
            auto parsed = parse_analysis_json(response);
            if (!parsed || trim(parsed->summary).empty()) return extractive(text);
            parsed->summary = trim(parsed->summary);
            parsed->filename = trim(parsed->filename);
            return parsed;
        } catch (const PromptBudgetError& error) {
            check_stopped();
            refresh_capacity(prompt, error, capacity);
            return std::nullopt;
        }
    }

    std::size_t fit_prefix(const std::string& text, bool merging, std::size_t unknown_chunk_bytes,
        const Capacity& capacity) const
    {
        if (!capacity.budget || !capacity.available) {
            return utf8_boundary(text, std::min(text.size(), unknown_chunk_bytes));
        }
        std::size_t low = 0, high = text.size();
        while (low < high) {
            const auto middle = low + (high - low + 1) / kChunkSplitDivisor;
            const auto prompt = build_prompt_(text.substr(0, utf8_boundary(text, middle)), merging);
            const auto measured = llm_.completion_budget(prompt, response_tokens_);
            if (measured && measured->prompt_tokens <= *capacity.available) low = middle;
            else high = middle - 1;
        }
        return utf8_boundary(text, low);
    }

    std::size_t chunk_length(const std::string& remaining, bool merging, std::size_t consumed,
        std::size_t unknown_chunk_bytes, const Capacity& capacity) const
    {
        auto length = fit_prefix(remaining, merging, unknown_chunk_bytes, capacity);
        // A rejected request must make progress even if the next measurement says it fits.
        if (!consumed && length == remaining.size()) length = utf8_boundary(remaining, length / kChunkSplitDivisor);
        if (!length) return 0;
        const auto space = remaining.rfind(' ', length - 1);
        const auto threshold = length * kWordBoundaryNumerator / kWordBoundaryDenominator;
        if (length < remaining.size() && space != std::string::npos && space > threshold) return space + 1;
        return length;
    }

    ParsedAnalysis summarize_parts(const std::string& text, bool merging, int depth, const Capacity& capacity)
    {
        const auto unknown_chunk_bytes = std::max<std::size_t>(1, text.size() / kChunkSplitDivisor);
        std::string notes;
        std::size_t consumed = 0;
        while (consumed < text.size()) {
            check_stopped();
            const auto remaining = text.substr(consumed);
            const auto length = chunk_length(remaining, merging, consumed, unknown_chunk_bytes, capacity);
            if (!length) return extractive(text);
            const auto part = summarize(remaining.substr(0, length), merging, depth + 1);
            if (!notes.empty()) notes += '\n';
            notes += part.summary;
            consumed += length;
        }
        if (notes.empty()) return extractive(text);
        return summarize(notes, true, depth + 1);
    }

    ParsedAnalysis summarize(const std::string& text, bool merging, int depth)
    {
        check_stopped();
        if (depth >= kMaximumSummaryDepth || result_.requests >= kMaximumSummaryRequests) return extractive(text);
        const auto prompt = build_prompt_(text, merging);
        Capacity capacity{llm_.completion_budget(prompt, response_tokens_), std::nullopt};
        capacity.available = input_capacity(capacity.budget);
        if (auto parsed = try_complete(text, prompt, merging, capacity)) return *parsed;
        return summarize_parts(text, merging, depth, capacity);
    }

    ILLMClient& llm_;
    const DocumentSummaryService::PromptBuilder& build_prompt_;
    int response_tokens_;
    const std::function<bool()>& should_stop_;
    DocumentSummaryResult result_;
};
} // namespace

DocumentSummaryResult DocumentSummaryService::summarize(const std::string& evidence, ILLMClient& llm,
    const PromptBuilder& build_prompt, int response_tokens, const std::function<bool()>& should_stop)
{
    return SummaryRun(llm, build_prompt, response_tokens, should_stop).run(evidence);
}
