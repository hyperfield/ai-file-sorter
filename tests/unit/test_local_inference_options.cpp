#include "LocalInferenceOptions.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace {
LocalInferenceOptions options(const std::map<std::string, std::string>& values)
{
    return LocalInferenceOptions::from_environment([&](const char* name) -> const char* {
        const auto it = values.find(name);
        return it == values.end() ? nullptr : it->second.c_str();
    });
}
} // namespace

TEST_CASE("Local inference options preserve shipped defaults")
{
    const auto result = options({});
    CHECK(result.temperature == 0.8f);
    CHECK(result.min_p == 0.05f);
    CHECK(result.seed == std::numeric_limits<std::uint32_t>::max());
    CHECK(result.threads == 0);
    CHECK(result.batch_tokens == 0);
    CHECK_FALSE(result.output_tokens);
    CHECK_FALSE(result.flash_attention);
}

TEST_CASE("Local inference options support reproducible per-client controls")
{
    const auto result = options({{"AI_FILE_SORTER_LLM_TEMPERATURE", "0"},
        {"AI_FILE_SORTER_LLM_SEED", "42"}, {"AI_FILE_SORTER_LLM_THREADS", "8"},
        {"AI_FILE_SORTER_LLM_BATCH_TOKENS", "256"}, {"AI_FILE_SORTER_LLM_MICROBATCH_TOKENS", "128"},
        {"AI_FILE_SORTER_LLM_OUTPUT_TOKENS", "256"}, {"AI_FILE_SORTER_LLM_FLASH_ATTENTION", "1"}});
    CHECK(result.temperature == 0);
    CHECK(result.seed == 42);
    CHECK(result.threads == 8);
    CHECK(result.batch_tokens == 256);
    CHECK(result.microbatch_tokens == 128);
    CHECK(result.output_tokens == 256);
    CHECK(result.flash_attention == true);
}

TEST_CASE("Local inference options reject malformed and unsafe overrides")
{
    for (const auto& value : {"nan", "inf", "-0.1", "11", "0.2junk", " 0"}) {
        CHECK_THROWS_AS(options({{"AI_FILE_SORTER_LLM_TEMPERATURE", value}}), std::invalid_argument);
    }
    for (const auto& value : {"-1", "4294967296", "42x"}) {
        CHECK_THROWS_AS(options({{"AI_FILE_SORTER_LLM_SEED", value}}), std::invalid_argument);
    }
    CHECK_THROWS_AS(options({{"AI_FILE_SORTER_LLM_THREADS", "-1"}}), std::invalid_argument);
    CHECK_THROWS_AS(options({{"AI_FILE_SORTER_LLM_OUTPUT_TOKENS", "0"}}), std::invalid_argument);
    CHECK_THROWS_AS(options({{"AI_FILE_SORTER_LLM_MIN_P", "1.1"}}), std::invalid_argument);
    CHECK_THROWS_AS(options({{"AI_FILE_SORTER_LLM_FLASH_ATTENTION", "yes"}}), std::invalid_argument);
    CHECK_THROWS_AS(options({{"AI_FILE_SORTER_LLM_BATCH_TOKENS", "128"},
        {"AI_FILE_SORTER_LLM_MICROBATCH_TOKENS", "256"}}), std::invalid_argument);
    LocalInferenceOptions invalid;
    invalid.temperature = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(invalid.validate(), std::invalid_argument);
}
