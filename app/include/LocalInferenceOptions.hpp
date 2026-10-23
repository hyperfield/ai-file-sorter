#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>

/** @brief Validated per-client text inference controls; defaults preserve shipped behavior. */
struct LocalInferenceOptions {
    /** @brief Sampling temperature; zero selects greedily. */
    float temperature{0.8f};
    /** @brief Minimum relative token probability. */
    float min_p{0.05f};
    /** @brief Sampling seed; maximum uint32 requests a random seed. */
    std::uint32_t seed{std::numeric_limits<std::uint32_t>::max()};
    /** @brief Generation and prompt workers, or zero for the runtime default. */
    int threads{0};
    /** @brief Logical prompt batch size, or zero for the app default. */
    int batch_tokens{0};
    /** @brief Physical prompt batch size, or zero for the runtime default. */
    int microbatch_tokens{0};
    /** @brief Flash-attention override; empty preserves backend defaults. */
    std::optional<bool> flash_attention;
    /** @brief Experiment output budget; empty honors each caller's budget. */
    std::optional<int> output_tokens;
    /** @brief Environment lookup, injectable without mutating process state. */
    using Environment = std::function<const char*(const char*)>;
    /**
     * @brief Resolve opt-in AI_FILE_SORTER_LLM_* controls.
     * @param read Environment lookup, or empty for the process environment.
     * @return Validated options; absent variables preserve defaults.
     * @throws std::invalid_argument For malformed, nonfinite or out-of-range values.
     */
    static LocalInferenceOptions from_environment(const Environment& read = {});
    /** @brief Validate injected controls. @throws std::invalid_argument For invalid values. */
    void validate() const;
};
