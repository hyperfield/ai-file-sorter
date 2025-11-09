#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include <string>
#include "llama.h"

namespace LocalLLMTestAccess {

enum class BackendPreference {
    Auto,
    Cpu,
    Cuda,
    Vulkan
};

BackendPreference detect_preferred_backend();
bool apply_cpu_backend(llama_model_params& params, BackendPreference preference);
bool apply_vulkan_backend(const std::string& model_path,
                          llama_model_params& params);
bool handle_cuda_forced_off(bool cuda_forced_off,
                            BackendPreference preference,
                            llama_model_params& params);
bool configure_cuda_backend(const std::string& model_path,
                            llama_model_params& params);
llama_model_params prepare_model_params_for_testing(const std::string& model_path);

} // namespace LocalLLMTestAccess

#endif // AI_FILE_SORTER_TEST_BUILD
