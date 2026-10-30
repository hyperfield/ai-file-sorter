#pragma once
#include "Types.hpp"
#include "LLMResponseConstraint.hpp"
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
    /**
     * @brief Categorize with a per-request decoding contract when supported by the backend.
     * @param file_name Item display name.
     * @param file_path Item context path.
     * @param file_type File or directory.
     * @param consistency_context Complete evidence and routing instructions.
     * @param constraint Structured response schema; native clients enforce it during decoding.
     * @return Raw response; callers must validate it even for constrained providers.
     * @note Legacy providers retain their existing request format and rely on caller-side validation.
     */
    virtual std::string categorize_file_constrained(const std::string& file_name,
        const std::string& file_path, FileType file_type, const std::string& consistency_context,
        const LLMResponseConstraint& /*constraint*/)
    {
        return categorize_file(file_name, file_path, file_type, consistency_context);
    }
    virtual std::string complete_prompt(const std::string& prompt,
                                        int max_tokens) = 0;
    virtual void set_prompt_logging_enabled(bool enabled) = 0;
};
