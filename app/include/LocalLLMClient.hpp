#pragma once

#include "ILLMClient.hpp"
#include "Types.hpp"
#include "llama.h"
#include <functional>
#include <memory>
#include <string>

namespace spdlog { class logger; }

class LocalLLMClient : public ILLMClient {
public:
    /**
     * @brief Status events emitted by the local LLM client.
     */
    enum class Status {
        /**
         * @brief GPU backend initialization failed; the client fell back to CPU.
         */
        GpuFallbackToCpu
    };
    /**
     * @brief Callback invoked when the local LLM client emits a status event.
     * @param status Status event emitted by the client.
     */
    using StatusCallback = std::function<void(Status)>;
    /**
     * @brief Callback invoked when a GPU failure occurs to decide whether to retry on CPU.
     * @param reason Short description of the failure cause.
     * @return True to retry on CPU; false to abort.
     */
    using FallbackDecisionCallback = std::function<bool(const std::string& reason)>;

    explicit LocalLLMClient(const std::string& model_path,
                            FallbackDecisionCallback fallback_decision_callback = {});
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
    /**
     * @brief Registers a status callback for runtime events.
     * @param callback Callback to invoke when status events occur.
     */
    void set_status_callback(StatusCallback callback);
    /**
     * @brief Registers a callback to decide whether GPU failures should fall back to CPU.
     * @param callback Callback to invoke when a GPU failure is detected.
     */
    void set_fallback_decision_callback(FallbackDecisionCallback callback);

private:
    void load_model_if_needed();
    void configure_llama_logging(const std::shared_ptr<spdlog::logger>& logger) const;
    llama_model_params prepare_model_params(const std::shared_ptr<spdlog::logger>& logger);
    llama_model_params load_model_or_throw(llama_model_params model_params,
                                           const std::shared_ptr<spdlog::logger>& logger);
    void configure_context(int context_length, const llama_model_params& model_params);
    /**
     * @brief Emits a status event to the registered callback.
     * @param status Status event to emit.
     */
    void notify_status(Status status) const;

    std::string model_path;
    llama_model* model;
    llama_context* ctx;
    const llama_vocab *vocab;
    llama_sampler* smpl;
    std::string sanitize_output(std::string &output);
    llama_context_params ctx_params;
    bool prompt_logging_enabled{false};
    StatusCallback status_callback_;
    FallbackDecisionCallback fallback_decision_callback_;
};
