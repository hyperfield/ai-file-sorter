#pragma once

#include <cstddef>
#include <functional>
#include <string>

class ILLMClient;

/** @brief Document evidence and diagnostics before filename normalization. */
struct DocumentSummaryResult {
    std::string summary;
    std::string filename;
    std::size_t requests{0};
    std::size_t source_chunks{0};
    bool used_extractive_fallback{false};
};

/** @brief Generate bounded document evidence using a provider's completion budget. */
namespace DocumentSummaryService {
/** @brief Build a source or merge prompt; true selects the merge instructions. */
using PromptBuilder = std::function<std::string(const std::string&, bool)>;

/**
 * @brief Summarize evidence, fitting source chunks and merging when needed.
 * @param evidence Bounded useful UTF-8 text extracted from one document.
 * @param llm Completion provider, including its native tokenizer when available.
 * @param build_prompt Shared production prompt builder for source and merge requests.
 * @param response_tokens Reserved completion output tokens.
 * @param should_stop Optional cooperative cancellation check.
 * @return Summary, proposed filename stem, and completion diagnostics.
 * @throws std::runtime_error On cancellation or provider errors other than input limits.
 */
DocumentSummaryResult summarize(const std::string& evidence, ILLMClient& llm,
    const PromptBuilder& build_prompt, int response_tokens,
    const std::function<bool()>& should_stop = {});
}
