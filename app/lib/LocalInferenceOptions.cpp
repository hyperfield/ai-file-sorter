#include "LocalInferenceOptions.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr float kMaximumTemperature = 10.0f;
constexpr int kMaximumWorkers = 1024;
constexpr int kMaximumTokenControl = 131072;

template <typename Value>
void read_control(const LocalInferenceOptions::Environment& read, const char* name, Value& value)
{
    const char* raw = read(name);
    if (!raw || !*raw) return;
    const std::string_view text(raw);
    Value parsed{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw std::invalid_argument(std::string("Invalid inference control: ") + name);
    }
    value = parsed;
}

void require(bool valid, const char* name)
{
    if (!valid) throw std::invalid_argument(std::string("Out-of-range inference control: ") + name);
}

bool finite_fraction(float value, float maximum)
{
    return std::isfinite(value) && value >= 0 && value <= maximum;
}

bool within_range(int value, int minimum, int maximum)
{
    return value >= minimum && value <= maximum;
}
} // namespace

LocalInferenceOptions LocalInferenceOptions::from_environment(const Environment& lookup)
{
    const Environment read = lookup ? lookup : Environment{[](const char* key) { return std::getenv(key); }};
    LocalInferenceOptions result;
    read_control(read, "AI_FILE_SORTER_LLM_TEMPERATURE", result.temperature);
    read_control(read, "AI_FILE_SORTER_LLM_MIN_P", result.min_p);
    read_control(read, "AI_FILE_SORTER_LLM_SEED", result.seed);
    read_control(read, "AI_FILE_SORTER_LLM_THREADS", result.threads);
    read_control(read, "AI_FILE_SORTER_LLM_BATCH_TOKENS", result.batch_tokens);
    read_control(read, "AI_FILE_SORTER_LLM_MICROBATCH_TOKENS", result.microbatch_tokens);
    int output = 0;
    read_control(read, "AI_FILE_SORTER_LLM_OUTPUT_TOKENS", output);
    if (const char* raw = read("AI_FILE_SORTER_LLM_OUTPUT_TOKENS"); raw && *raw) result.output_tokens = output;
    if (const char* raw = read("AI_FILE_SORTER_LLM_FLASH_ATTENTION"); raw && *raw) {
        const std::string_view text(raw);
        require(text == "0" || text == "1", "flash_attention");
        result.flash_attention = text == "1";
    }
    result.validate();
    return result;
}

void LocalInferenceOptions::validate() const
{
    require(finite_fraction(temperature, kMaximumTemperature), "temperature");
    require(finite_fraction(min_p, 1), "min_p");
    require(within_range(threads, 0, kMaximumWorkers), "threads");
    require(within_range(batch_tokens, 0, kMaximumTokenControl), "batch_tokens");
    require(within_range(microbatch_tokens, 0, kMaximumTokenControl), "microbatch_tokens");
    require(!batch_tokens || microbatch_tokens <= batch_tokens, "microbatch_tokens > batch_tokens");
    if (output_tokens) require(within_range(*output_tokens, 1, kMaximumTokenControl), "output_tokens");
}
