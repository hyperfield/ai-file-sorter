#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "CategoryLanguage.hpp"
#include "FilenameLocalizationService.hpp"
#include "ILLMClient.hpp"

#include <string>

namespace {

class FixedResponseLLM : public ILLMClient {
public:
    explicit FixedResponseLLM(std::string response)
        : response_(std::move(response))
    {
    }

    std::string categorize_file(const std::string&,
                                const std::string&,
                                FileType,
                                const std::string&) override
    {
        return std::string();
    }

    std::string complete_prompt(const std::string& prompt, int) override
    {
        last_prompt = prompt;
        return response_;
    }

    void set_prompt_logging_enabled(bool) override
    {
    }

    std::string last_prompt;

private:
    std::string response_;
};

} // namespace

TEST_CASE("FilenameLocalizationService localizes filename stems and preserves extensions")
{
    FilenameLocalizationService service;
    FixedResponseLLM llm("lac_d_hiver");

    const std::string localized =
        service.localize_filename("winter_lake_scene.jpg", CategoryLanguage::French, llm);

    CHECK(localized == "lac_d_hiver.jpg");
    CHECK(llm.last_prompt.find("French") != std::string::npos);
    CHECK(llm.last_prompt.find("winter_lake_scene") != std::string::npos);
    CHECK(llm.last_prompt.find("no more than 3 words") != std::string::npos);
}

TEST_CASE("FilenameLocalizationService keeps the original suggestion when localization is unusable")
{
    FilenameLocalizationService service;
    FixedResponseLLM llm("!!!");

    const std::string localized =
        service.localize_filename("receipt_summary.pdf", CategoryLanguage::German, llm);

    CHECK(localized == "receipt_summary.pdf");
}

TEST_CASE("FilenameLocalizationService skips localization when English is selected")
{
    FilenameLocalizationService service;
    FixedResponseLLM llm("should_not_be_used");

    const std::string localized =
        service.localize_filename("project_notes.txt", CategoryLanguage::English, llm);

    CHECK(localized == "project_notes.txt");
    CHECK(llm.last_prompt.empty());
}
