#pragma once

#include <filesystem>
#include <optional>
#include <string>

/**
 * @brief Resolves local visual/text LLM runtime files and fallback heuristics.
 */
class VisualLlmRuntime {
public:
    struct Paths {
        std::filesystem::path model_path;
        std::filesystem::path mmproj_path;
    };

    static bool default_text_llm_files_available();
    static std::optional<Paths> resolve_paths(std::string* error = nullptr);
    static bool should_use_gpu();
    static bool should_offer_cpu_fallback(const std::string& reason);
};
