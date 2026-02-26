#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <curl/curl.h>

#include "Utils.hpp"

namespace TestHooks {

struct BackendMemoryInfo {
    Utils::CudaMemoryInfo memory;
    bool is_integrated = false;
    std::string name;
};

using BackendMemoryProbe = std::function<std::optional<BackendMemoryInfo>(std::string_view backend_name)>;
void set_backend_memory_probe(BackendMemoryProbe probe);
void reset_backend_memory_probe();

using BackendAvailabilityProbe = std::function<bool(std::string_view backend_name)>;
void set_backend_availability_probe(BackendAvailabilityProbe probe);
void reset_backend_availability_probe();

using CudaAvailabilityProbe = std::function<bool()>;
void set_cuda_availability_probe(CudaAvailabilityProbe probe);
void reset_cuda_availability_probe();

using CudaMemoryProbe = std::function<std::optional<Utils::CudaMemoryInfo>()>;
void set_cuda_memory_probe(CudaMemoryProbe probe);
void reset_cuda_memory_probe();

struct CategorizationMoveInfo {
    bool show_subcategory_folders;
    std::string category;
    std::string subcategory;
    std::string file_name;
};

using CategorizationMoveProbe = std::function<void(const CategorizationMoveInfo&)>;
void set_categorization_move_probe(CategorizationMoveProbe probe);
void reset_categorization_move_probe();

#ifdef AI_FILE_SORTER_TEST_BUILD
using LLMDownloadProbe = std::function<CURLcode(long resume_offset, const std::string& destination_path)>;
void set_llm_download_probe(LLMDownloadProbe probe);
void reset_llm_download_probe();
#endif

} // namespace TestHooks
