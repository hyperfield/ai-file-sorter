#include "LlmCatalog.hpp"

#include <QObject>

#include <algorithm>
#include <cstdlib>

namespace {
QString resolved_llm_name(const DefaultLlmEntry& entry)
{
    const char* env_value = std::getenv(entry.name_env);
    if (env_value && *env_value) {
        return QString::fromUtf8(env_value);
    }
    return QString::fromUtf8(entry.fallback_name);
}
} // namespace

const std::vector<DefaultLlmEntry>& default_llm_entries()
{
    static const std::vector<DefaultLlmEntry> entries = {
        {LLMChoice::Local_3b, "LOCAL_LLM_3B_DOWNLOAD_URL", "LOCAL_LLM_3B_DISPLAY_NAME",
         "LLaMa 3b v3.2 Instruct Q4"},
        {LLMChoice::Local_3b_legacy, "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL",
         "LOCAL_LLM_3B_LEGACY_DISPLAY_NAME", "LLaMa 3b v3.2 Instruct Q8, legacy"},
        {LLMChoice::Local_7b, "LOCAL_LLM_7B_DOWNLOAD_URL", "LOCAL_LLM_7B_DISPLAY_NAME",
         "Mistral 7b Instruct v0.2 Q5"}};
    return entries;
}

QString default_llm_label(const DefaultLlmEntry& entry)
{
    return QObject::tr("Local LLM (%1)").arg(resolved_llm_name(entry));
}

QString default_llm_label_for_choice(LLMChoice choice)
{
    const auto& entries = default_llm_entries();
    const auto it = std::find_if(entries.begin(), entries.end(),
                                 [choice](const DefaultLlmEntry& entry) {
                                     return entry.choice == choice;
                                 });
    if (it == entries.end()) {
        return QObject::tr("Local LLM");
    }
    return default_llm_label(*it);
}
