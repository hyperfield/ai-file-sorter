#include <catch2/catch_test_macros.hpp>

#include "CategoryLanguageSupport.hpp"

TEST_CASE("Gemma 3 category language support exposes the full language list")
{
    REQUIRE(all_category_languages().size() > 17);
    CHECK(all_category_languages().front() == CategoryLanguage::Afrikaans);
    CHECK(all_category_languages().back() == CategoryLanguage::Zulu);
    CHECK(supported_category_languages_for_llm_choice(LLMChoice::Local_4b_Gemma)
          == all_category_languages());
}

TEST_CASE("Qwen category language support exposes the full language list")
{
    CHECK(supported_category_languages_for_llm_choice(LLMChoice::Local_4b_Qwen35)
          == all_category_languages());
}

TEST_CASE("Custom local models keep the full category language list")
{
    CHECK(supported_category_languages_for_llm_choice(LLMChoice::Custom)
          == all_category_languages());
}

TEST_CASE("Mistral 7B category language support stays limited to the supported subset")
{
    const auto& languages = supported_category_languages_for_llm_choice(LLMChoice::Local_7b);

    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::English));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Dutch));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::French));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::German));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Italian));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Swedish));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Norwegian));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Danish));
    CHECK(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Spanish));

    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Hindi));
    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Polish));
    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Portuguese));
    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::SimplifiedChinese));
    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Icelandic));
    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Finnish));
    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Turkish));
    CHECK_FALSE(is_category_language_supported_for_llm_choice(LLMChoice::Local_7b, CategoryLanguage::Korean));

    CHECK(languages.size() == 9);
}

TEST_CASE("English-only built-in local models expose only English")
{
    const auto& legacy_languages =
        supported_category_languages_for_llm_choice(LLMChoice::Local_3b_legacy);
    const auto& gemma_7b_languages =
        supported_category_languages_for_llm_choice(LLMChoice::Local_7b_Gemma);

    REQUIRE(legacy_languages.size() == 1);
    CHECK(legacy_languages.front() == CategoryLanguage::English);
    REQUIRE(gemma_7b_languages.size() == 1);
    CHECK(gemma_7b_languages.front() == CategoryLanguage::English);
}
