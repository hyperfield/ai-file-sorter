#pragma once

#include "ILLMClient.hpp"
#include "Types.hpp"
#include "llama.h"
#include <memory>
#include <string>

namespace spdlog { class logger; }

class LocalLLMClient : public ILLMClient {
public:
    explicit LocalLLMClient(const std::string& model_path);
    ~LocalLLMClient();

    std::string make_prompt(const std::string& file_name,
                            const std::string& file_path,
                            FileType file_type,
                            const std::string& consistency_context);
    std::string generate_response(const std::string &prompt, int n_predict, bool apply_sanitizer = true);
    std::string categorize_file(const std::string& file_name,
                                const std::string& file_path,
                                FileType file_type,
                                const std::string& consistency_context) override;
    std::string complete_prompt(const std::string& prompt,
                                int max_tokens) override;
    void set_prompt_logging_enabled(bool enabled) override;

private:
    void load_model_if_needed();
    void configure_llama_logging(const std::shared_ptr<spdlog::logger>& logger) const;
    llama_model_params prepare_model_params(const std::shared_ptr<spdlog::logger>& logger);
    void load_model_or_throw(const llama_model_params& model_params,
                             const std::shared_ptr<spdlog::logger>& logger);
    void configure_context(int context_length, const llama_model_params& model_params);

    std::string model_path;
    llama_model* model;
    llama_context* ctx;
    const llama_vocab *vocab;
    llama_sampler* smpl;
    std::string sanitize_output(std::string &output);
    llama_context_params ctx_params;
    bool prompt_logging_enabled{false};
};
