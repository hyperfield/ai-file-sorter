#pragma once

#include "ILLMClient.hpp"
#include "Types.hpp"
#include "llama.h"
#include <string>

class LocalLLMClient : public ILLMClient {
public:
    explicit LocalLLMClient(const std::string& model_path);
    ~LocalLLMClient();

    std::string make_prompt(const std::string& file_name,
                            const std::string& file_path,
                            FileType file_type);
    std::string generate_response(const std::string &prompt, int n_predict);
    std::string categorize_file(const std::string& file_name,
                                const std::string& file_path,
                                FileType file_type) override;
    std::string complete_prompt(const std::string& prompt,
                                int max_tokens) override;

private:
    void load_model_if_needed();

    std::string model_path;
    llama_model* model;
    llama_context* ctx;
    const llama_vocab *vocab;
    llama_sampler* smpl;
    std::string sanitize_output(std::string &output);
    llama_context_params ctx_params;
};
