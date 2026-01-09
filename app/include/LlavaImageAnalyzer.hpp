#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#ifdef AI_FILE_SORTER_HAS_MTMD
#include "ggml.h"

struct llama_model;
struct llama_context;
struct llama_vocab;
struct mtmd_context;
struct mtmd_bitmap;
#endif

struct LlavaImageAnalysisResult {
    std::string description;
    std::string suggested_name;
};

class LlavaImageAnalyzer {
public:
    struct Settings {
        int32_t n_ctx = 4096;
        int32_t n_predict = 80;
        int32_t n_threads = 0;
        float temperature = 0.2f;
        bool use_gpu = true;
        std::function<void(int32_t current_batch, int32_t total_batches)> batch_progress;
    };

    LlavaImageAnalyzer(const std::filesystem::path& model_path,
                       const std::filesystem::path& mmproj_path,
                       Settings settings);
    LlavaImageAnalyzer(const std::filesystem::path& model_path,
                       const std::filesystem::path& mmproj_path);
    ~LlavaImageAnalyzer();

    LlavaImageAnalyzer(const LlavaImageAnalyzer&) = delete;
    LlavaImageAnalyzer& operator=(const LlavaImageAnalyzer&) = delete;

    LlavaImageAnalysisResult analyze(const std::filesystem::path& image_path);

    static bool is_supported_image(const std::filesystem::path& path);

private:
    std::string build_description_prompt() const;
    std::string build_filename_prompt(const std::string& description) const;
#ifdef AI_FILE_SORTER_HAS_MTMD
    std::string infer_text(mtmd_bitmap* bitmap,
                           const std::string& prompt,
                           int32_t max_tokens);
#else
    std::string infer_text(void* bitmap,
                           const std::string& prompt,
                           int32_t max_tokens);
#endif
    std::string sanitize_filename(const std::string& value,
                                  size_t max_words,
                                  size_t max_length) const;

    static std::string trim(std::string value);
    static std::string slugify(const std::string& value);
    static std::string normalize_filename(const std::string& base,
                                          const std::filesystem::path& original_path);

#ifdef AI_FILE_SORTER_HAS_MTMD
    llama_model* model_{nullptr};
    llama_context* context_{nullptr};
    mtmd_context* vision_ctx_{nullptr};
    const llama_vocab* vocab_{nullptr};
    std::filesystem::path model_path_;
    std::filesystem::path mmproj_path_;
    std::optional<bool> visual_gpu_override_;
    int32_t context_tokens_{0};
    int32_t batch_size_{512};
    bool text_gpu_enabled_{false};
    bool mmproj_gpu_enabled_{false};
    static void mtmd_progress_callback(const char* name,
                                       int32_t current_batch,
                                       int32_t total_batches,
                                       void* user_data);
#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
    static void mtmd_log_callback(enum ggml_log_level level,
                                  const char* text,
                                  void* user_data);
#endif
#endif
    Settings settings_;
};
