#pragma once

#include "ILLMClient.hpp"
#include "ILLMRequestControl.hpp"
#include "LocalInferenceOptions.hpp"
#include "Types.hpp"
#include "llama.h"
#include <functional>
#include <atomic>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace spdlog { class logger; }

class LocalLLMClient : public ILLMClient, public ILLMRequestControl {
public:
    /**
     * @brief Per-client local runtime options.
     */
    struct Options {
        /**
         * @brief Force this local client to load and run on CPU regardless of process GPU settings.
         */
        bool force_cpu_backend{false};
        /** @brief Injected controls, or empty to resolve opt-in environment settings. */
        std::optional<LocalInferenceOptions> inference;
    };

    /**
     * @brief Status events emitted by the local LLM client.
     */
    enum class Status {
        /**
         * @brief Available GPU memory is too low; the client fell back to CPU before loading.
         */
        GpuLowMemoryFallbackToCpu,
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

    explicit LocalLLMClient(const std::string& model_path);
    LocalLLMClient(const std::string& model_path,
                   FallbackDecisionCallback fallback_decision_callback);
    LocalLLMClient(const std::string& model_path,
                   FallbackDecisionCallback fallback_decision_callback,
                   Options options);
    ~LocalLLMClient();

    std::string make_prompt(const std::string& file_name,
                            const std::string& file_path,
                            FileType file_type,
                            const std::string& consistency_context);
    std::string generate_response(const std::string& prompt,
                                  int n_predict,
                                  bool apply_sanitizer = true,
                                  const std::string& system_prompt = {},
                                  const LLMResponseConstraint& constraint = {});
    std::string categorize_file(const std::string& file_name,
                                const std::string& file_path,
                                FileType file_type,
                                const std::string& consistency_context) override;
    /**
     * @brief Categorize with a per-request schema enforced by the native grammar sampler.
     * @param file_name Item name.
     * @param file_path Source path without embedded evidence.
     * @param file_type Item kind.
     * @param consistency_context Complete routing instructions and content evidence.
     * @param constraint Structured response contract; empty leaves generation unconstrained.
     * @return Unsanitized structured reply for folder routing.
     * @throws ResponseConstraintError If the requested constraint cannot be installed.
     */
    std::string categorize_file_constrained(const std::string& file_name,
        const std::string& file_path, FileType file_type, const std::string& consistency_context,
        const LLMResponseConstraint& constraint) override;
    std::string complete_prompt(const std::string& prompt,
                                int max_tokens) override;
    void set_prompt_logging_enabled(bool enabled) override;
    /**
     * @brief Set or clear the cooperative deadline for a timed inference request.
     * @param deadline Monotonic deadline, or empty to disable cancellation.
     */
    void set_request_deadline(std::optional<Clock::time_point> deadline) noexcept override;
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
    /**
     * @brief Check the request deadline without touching backend objects from another thread.
     * @param client Owning LocalLLMClient, passed through the llama.cpp callback.
     * @return True after its request deadline expires.
     */
    static bool request_expired(void* client);
    std::atomic<Clock::duration::rep> request_deadline_{std::numeric_limits<Clock::duration::rep>::max()};
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
    void notify_status(Status status);

    std::string model_path;
    llama_model* model;
    llama_context* ctx;
    const llama_vocab *vocab;
    llama_sampler* smpl;
    std::string sanitize_output(const std::string& output);
    llama_context_params ctx_params;
    bool prompt_logging_enabled{false};
    Options options_;
    LocalInferenceOptions inference_;
    StatusCallback status_callback_;
    FallbackDecisionCallback fallback_decision_callback_;
    std::vector<Status> pending_statuses_;
    std::optional<std::string> original_gpu_backend_env_;
    std::optional<std::string> original_llama_arg_device_env_;
    std::optional<std::string> original_ggml_disable_cuda_env_;
};
