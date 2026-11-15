// llm_cost.hpp is vendored from https://github.com/Mattbusel/llm-cost (commit f0f858f).
//
// MIT License
//
// Copyright (c) 2026 Mattbusel
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

// llm_cost.hpp: Zero-dependency single-header C++ token counter and cost estimator.
// Supports OpenAI and Anthropic models. No network calls. No external deps.
//
// USAGE:
//   In exactly ONE .cpp file:
//     #define LLM_COST_IMPLEMENTATION
//     #include "llm_cost.hpp"
//
//   In all other files:
//     #include "llm_cost.hpp"
//
// Token counting uses a cl100k_base approximation (~±5% vs tiktoken). See README.

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace llm {

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

enum class Provider { OpenAI, Anthropic };

/// Pricing and metadata for one model.
struct Model {
    std::string name;
    Provider    provider;
    double      input_cost_per_1k;   ///< USD per 1 000 input tokens
    double      output_cost_per_1k;  ///< USD per 1 000 output tokens
    size_t      context_window;      ///< Maximum context in tokens
};

// ---------------------------------------------------------------------------
// Built-in model registry
//
// Prices are USD per 1K tokens at each provider's standard (non-batch,
// non-cached) rate, checked on 2026-09-25 against:
//   https://platform.claude.com/docs/en/about-claude/pricing
//   https://platform.claude.com/docs/en/about-claude/models/overview
//   https://developers.openai.com/api/docs/pricing
// Prices change; re-check those pages or define your own llm::Model.
// ---------------------------------------------------------------------------
namespace models {
    // OpenAI
    inline const Model GPT6_ASTRA    = {"gpt-6-astra",            Provider::OpenAI,    0.010,   0.050,   1050000};
    inline const Model GPT6_SOL      = {"gpt-6-sol",              Provider::OpenAI,    0.002,   0.010,   1050000};
    inline const Model GPT6_LUNA     = {"gpt-6-luna",             Provider::OpenAI,    0.0001,  0.0005,  1050000};
    inline const Model GPT4O         = {"gpt-4o",                 Provider::OpenAI,    0.0025,  0.010,   128000};
    inline const Model GPT4O_MINI    = {"gpt-4o-mini",            Provider::OpenAI,    0.00015, 0.0006,  128000};
    inline const Model GPT4_TURBO    = {"gpt-4-turbo",            Provider::OpenAI,    0.01,    0.03,    128000};
    // Anthropic
    inline const Model CLAUDE_FABLE_5_1 = {"claude-fable-5-1",    Provider::Anthropic, 0.010,   0.050,   1000000};
    inline const Model CLAUDE_OPUS_5_5  = {"claude-opus-5-5",     Provider::Anthropic, 0.004,   0.020,   1000000};
    inline const Model CLAUDE_SONNET_5  = {"claude-sonnet-5",     Provider::Anthropic, 0.002,   0.010,   1000000};
    inline const Model CLAUDE_OPUS   = {"claude-opus-4-5",        Provider::Anthropic, 0.005,   0.025,   200000};
    inline const Model CLAUDE_SONNET = {"claude-sonnet-4-5",      Provider::Anthropic, 0.003,   0.015,   200000};
    inline const Model CLAUDE_HAIKU  = {"claude-haiku-4-5",       Provider::Anthropic, 0.001,   0.005,   200000};
} // namespace models

/// Result of a token counting operation.
struct TokenCount {
    size_t      tokens;                 ///< Estimated token count
    size_t      characters;            ///< Raw character count
    double      estimated_cost_usd;    ///< Input cost only (output unknown pre-call)
    bool        exceeds_context;       ///< True if tokens > model.context_window
    std::string model_name;
};

/// One row in a cross-model cost comparison.
struct CostComparison {
    std::string model_name;
    size_t      tokens;
    double      input_cost_usd;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Estimate token count and input cost for a single text string.
///
/// Uses a cl100k_base heuristic: ~4 chars/token for prose, adjusted for
/// code, numbers, and non-ASCII. Accuracy is approximately ±5% vs tiktoken.
///
/// # Arguments
/// * `text`: The string to analyse
/// * `model`: Model to price against
///
/// # Panics
/// This function never panics.
TokenCount count(const std::string& text, const Model& model);

/// Estimate token count for a chat-format message list.
/// Adds per-message overhead (role tokens + 4 tokens/message, as OpenAI does).
///
/// # Arguments
/// * `messages`: Vector of {role, content} pairs
/// * `model`: Model to price against
///
/// # Panics
/// This function never panics.
TokenCount count_messages(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const Model& model
);

/// Throw std::runtime_error if estimated input cost exceeds budget_usd.
/// Call this before making an LLM API call to enforce cost limits.
///
/// # Throws
/// `std::runtime_error` if tc.estimated_cost_usd > budget_usd
void assert_budget(const TokenCount& tc, double budget_usd);

/// Compare input cost across all six built-in models for the same text.
/// Results are sorted cheapest-first.
///
/// # Arguments
/// * `text`: The prompt to price across all models
///
/// # Panics
/// This function never panics.
std::vector<CostComparison> compare_costs(const std::string& text);

/// Format a USD cost as a human-readable string.
/// Values >= $0.01 → "$0.0123"
/// Values <  $0.01 → "0.12¢"
/// Values == $0.00 → "$0.0000"
///
/// # Panics
/// This function never panics.
std::string format_cost(double usd);

} // namespace llm

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------
#ifdef LLM_COST_IMPLEMENTATION

namespace llm {
namespace detail {

// ---------------------------------------------------------------------------
// cl100k_base token count approximation.
//
// Strategy (mirrors tiktoken's rough behaviour without the BPE table):
//   1. Split on whitespace boundaries.
//   2. For each "word", estimate sub-tokens:
//      - Pure ASCII alphabetic:   1 token per ~4 chars (English prose)
//      - Numeric runs:            1 token per ~3 chars
//      - Punctuation / symbols:   1 token each (usually)
//      - Non-ASCII (unicode):     1 token per ~2 bytes (conservative)
//   3. Add a fixed overhead for leading whitespace tokens.
//
// Typical accuracy: ±5% vs tiktoken on English prose, ±10% on mixed code.
// ---------------------------------------------------------------------------
static size_t estimate_tokens(std::string_view text) {
    if (text.empty()) return 0;

    size_t tokens = 0;
    size_t i      = 0;
    const size_t n = text.size();

    while (i < n) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        // Whitespace: each run of whitespace is ~1 token
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++tokens;
            while (i < n) {
                unsigned char wc = static_cast<unsigned char>(text[i]);
                if (wc != ' ' && wc != '\t' && wc != '\n' && wc != '\r') break;
                ++i;
            }
            continue;
        }

        // Numeric run: digits encode at ~3 chars/token
        if (c >= '0' && c <= '9') {
            size_t start = i;
            while (i < n) {
                unsigned char dc = static_cast<unsigned char>(text[i]);
                if (dc < '0' || dc > '9') break;
                ++i;
            }
            size_t len = i - start;
            tokens += (len + 2) / 3; // ceil(len/3)
            continue;
        }

        // ASCII alphabetic run: ~4 chars/token
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            size_t start = i;
            while (i < n) {
                unsigned char ac = static_cast<unsigned char>(text[i]);
                if (!((ac >= 'a' && ac <= 'z') ||
                      (ac >= 'A' && ac <= 'Z') ||
                      (ac >= '0' && ac <= '9') ||
                       ac == '_'))
                    break;
                ++i;
            }
            size_t len = i - start;
            tokens += (len + 3) / 4; // ceil(len/4)
            continue;
        }

        // Non-ASCII (multi-byte UTF-8): conservative, 1 token per 2 bytes
        if (c >= 0x80) {
            size_t start = i;
            while (i < n && static_cast<unsigned char>(text[i]) >= 0x80) ++i;
            size_t bytes = i - start;
            tokens += (bytes + 1) / 2;
            continue;
        }

        // Single ASCII punctuation / symbol: 1 token each
        ++tokens;
        ++i;
    }

    // Apply a small empirical correction factor (tiktoken merges some pairs)
    // Multiply by 0.85 to account for BPE merges that our heuristic misses.
    tokens = static_cast<size_t>(std::ceil(tokens * 0.85));

    return tokens == 0 ? 1 : tokens;
}

static const Model& all_models_array(size_t idx) {
    static const Model arr[] = {
        models::GPT6_ASTRA,
        models::GPT6_SOL,
        models::GPT6_LUNA,
        models::GPT4O,
        models::GPT4O_MINI,
        models::GPT4_TURBO,
        models::CLAUDE_FABLE_5_1,
        models::CLAUDE_OPUS_5_5,
        models::CLAUDE_SONNET_5,
        models::CLAUDE_OPUS,
        models::CLAUDE_SONNET,
        models::CLAUDE_HAIKU,
    };
    return arr[idx];
}
static constexpr size_t ALL_MODELS_COUNT = 12;

} // namespace detail

// ---------------------------------------------------------------------------
// Public API implementations
// ---------------------------------------------------------------------------

TokenCount count(const std::string& text, const Model& model) {
    const size_t tokens    = detail::estimate_tokens(text);
    const double cost      = (tokens / 1000.0) * model.input_cost_per_1k;
    const bool   exceeds   = tokens > model.context_window;

    TokenCount tc;
    tc.tokens              = tokens;
    tc.characters          = text.size();
    tc.estimated_cost_usd  = cost;
    tc.exceeds_context     = exceeds;
    tc.model_name          = model.name;
    return tc;
}

TokenCount count_messages(
    const std::vector<std::pair<std::string, std::string>>& messages,
    const Model& model)
{
    // OpenAI overhead: 4 tokens per message, 3 tokens for role, +3 for reply priming
    size_t tokens = 3; // reply priming
    for (const auto& [role, content] : messages) {
        tokens += 4; // per-message overhead
        tokens += detail::estimate_tokens(role);
        tokens += detail::estimate_tokens(content);
    }

    const double cost    = (tokens / 1000.0) * model.input_cost_per_1k;
    const bool   exceeds = tokens > model.context_window;

    // Compute total chars for reference
    size_t chars = 0;
    for (const auto& [role, content] : messages)
        chars += role.size() + content.size();

    TokenCount tc;
    tc.tokens             = tokens;
    tc.characters         = chars;
    tc.estimated_cost_usd = cost;
    tc.exceeds_context    = exceeds;
    tc.model_name         = model.name;
    return tc;
}

void assert_budget(const TokenCount& tc, double budget_usd) {
    if (tc.estimated_cost_usd > budget_usd) {
        std::ostringstream oss;
        oss << "Budget exceeded: estimated " << format_cost(tc.estimated_cost_usd)
            << " > limit " << format_cost(budget_usd)
            << " (" << tc.tokens << " tokens on " << tc.model_name << ")";
        throw std::runtime_error(oss.str());
    }
}

std::vector<CostComparison> compare_costs(const std::string& text) {
    std::vector<CostComparison> results;
    results.reserve(detail::ALL_MODELS_COUNT);

    for (size_t i = 0; i < detail::ALL_MODELS_COUNT; ++i) {
        const Model& m = detail::all_models_array(i);
        const size_t tok  = detail::estimate_tokens(text);
        const double cost = (tok / 1000.0) * m.input_cost_per_1k;

        CostComparison cc;
        cc.model_name    = m.name;
        cc.tokens        = tok;
        cc.input_cost_usd = cost;
        results.push_back(cc);
    }

    std::sort(results.begin(), results.end(),
        [](const CostComparison& a, const CostComparison& b) {
            return a.input_cost_usd < b.input_cost_usd;
        });

    return results;
}

std::string format_cost(double usd) {
    if (usd == 0.0) {
        return "$0.0000";
    }
    std::ostringstream oss;
    if (usd >= 0.01) {
        oss << "$" << std::fixed << std::setprecision(4) << usd;
    } else {
        // Convert to cents
        oss << std::fixed << std::setprecision(4) << (usd * 100.0) << "\xC2\xA2"; // UTF-8 ¢
    }
    return oss.str();
}

} // namespace llm

#endif // LLM_COST_IMPLEMENTATION
