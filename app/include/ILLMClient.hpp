#pragma once
#include "Types.hpp"
#include <string>
#include <string_view>

namespace LLMPrompt {
/** @brief Delimits untrusted content evidence before routing instructions and candidate lists. */
inline constexpr std::string_view kContentEvidenceEndMarker = "\n[[AI_FILE_SORTER_CONTENT_EVIDENCE_END]]";
}

class ILLMClient {
public:
    virtual ~ILLMClient() = default;
    virtual std::string categorize_file(const std::string& file_name,
                                        const std::string& file_path,
                                        FileType file_type,
                                        const std::string& consistency_context) = 0;
    virtual std::string complete_prompt(const std::string& prompt,
                                        int max_tokens) = 0;
    virtual void set_prompt_logging_enabled(bool enabled) = 0;
};
