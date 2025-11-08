#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

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

using CudaAvailabilityProbe = std::function<bool()>;
void set_cuda_availability_probe(CudaAvailabilityProbe probe);
void reset_cuda_availability_probe();

using CudaMemoryProbe = std::function<std::optional<Utils::CudaMemoryInfo>()>;
void set_cuda_memory_probe(CudaMemoryProbe probe);
void reset_cuda_memory_probe();

} // namespace TestHooks
