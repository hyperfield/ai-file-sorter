#include "CategoryLanguageSupport.hpp"

#include <algorithm>

namespace {

const std::vector<CategoryLanguage>& all_languages()
{
    static const std::vector<CategoryLanguage> languages = [] {
        std::vector<CategoryLanguage> items;
        items.reserve(kCategoryLanguageCount);
        for (std::size_t idx = 0; idx < kCategoryLanguageCount; ++idx) {
            items.push_back(static_cast<CategoryLanguage>(idx));
        }
        return items;
    }();
    return languages;
}

const std::vector<CategoryLanguage>& mistral_7b_languages()
{
    static const std::vector<CategoryLanguage> languages = {
        CategoryLanguage::Danish,
        CategoryLanguage::Dutch,
        CategoryLanguage::English,
        CategoryLanguage::French,
        CategoryLanguage::German,
        CategoryLanguage::Italian,
        CategoryLanguage::Norwegian,
        CategoryLanguage::Spanish,
        CategoryLanguage::Swedish
    };
    return languages;
}

const std::vector<CategoryLanguage>& english_only_languages()
{
    static const std::vector<CategoryLanguage> languages = {
        CategoryLanguage::English
    };
    return languages;
}

} // namespace

const std::vector<CategoryLanguage>& all_category_languages()
{
    return all_languages();
}

const std::vector<CategoryLanguage>& supported_category_languages_for_llm_choice(LLMChoice choice)
{
    switch (choice) {
    case LLMChoice::Local_4b_Gemma:
    case LLMChoice::Local_4b_Qwen35:
        return all_languages();
    case LLMChoice::Local_7b:
        return mistral_7b_languages();
    case LLMChoice::Local_3b_legacy:
    case LLMChoice::Local_7b_Gemma:
        return english_only_languages();
    case LLMChoice::Custom:
        return all_languages();
    default:
        return all_languages();
    }
}

bool is_category_language_supported_for_llm_choice(LLMChoice choice, CategoryLanguage language)
{
    const auto& supported = supported_category_languages_for_llm_choice(choice);
    return std::find(supported.begin(), supported.end(), language) != supported.end();
}
