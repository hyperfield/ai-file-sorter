#include <catch2/catch_test_macros.hpp>

#include "NativeChatTemplate.hpp"

#include <string>

namespace {
constexpr char kTemplate[] =
    "{% for message in messages %}"
    "{{ '<|' + message['role'] + '|>\\n' + message['content'] + '\\n' }}"
    "{% endfor %}"
    "{% if add_generation_prompt %}{{ '<|assistant|>\\n' }}{% endif %}"
    "{% if enable_thinking %}{{ 'THINKING_ENABLED' }}{% else %}{{ 'THINKING_DISABLED' }}{% endif %}";
}

TEST_CASE("Native chat templates preserve messages and disable thinking")
{
    const auto prompt = NativeChatTemplate::render(kTemplate, "Sort accurately.", "Report.pdf");
    CHECK(prompt.find("<|system|>\nSort accurately.\n") != std::string::npos);
    CHECK(prompt.find("<|user|>\nReport.pdf\n") != std::string::npos);
    CHECK(prompt.find("<|assistant|>\n") != std::string::npos);
    CHECK(prompt.find("THINKING_DISABLED") != std::string::npos);
    CHECK(prompt.find("THINKING_ENABLED") == std::string::npos);
}

TEST_CASE("Native chat templates retain long folder lists and media markers")
{
    constexpr int kCandidateCount = 1000;
    std::string evidence = "<__media__>\n";
    for (int candidate = 0; candidate < kCandidateCount; ++candidate) {
        evidence += "Folder " + std::to_string(candidate) + "\n";
    }
    const auto prompt = NativeChatTemplate::render(kTemplate, "", evidence);
    CHECK(prompt.find(evidence) != std::string::npos);
    CHECK(prompt.find("<|system|>") == std::string::npos);
    CHECK(prompt.find("Folder 999\n") != std::string::npos);
}

TEST_CASE("Native chat templates reject missing models and empty template sources")
{
    CHECK_THROWS(NativeChatTemplate::format(nullptr, "system", "user"));
    CHECK_THROWS(NativeChatTemplate::render("", "system", "user"));
}
