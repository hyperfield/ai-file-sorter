#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

/** @brief Loads the selected Windows runtime after backend validation. */
namespace WindowsGgmlRuntime {

/** @brief Identifies a runtime DLL that Windows could not load. */
struct LoadError {
    std::filesystem::path library_path;
    std::uint32_t code;
};

/**
 * @brief Preloads the app's delayed GGML/llama imports from one runtime directory.
 *
 * Uses an absolute path and searches the runtime directory before other DLL
 * locations. Successful modules stay loaded for the process lifetime, as normal
 * executable imports would. Call after validating/preloading the Vulkan loader
 * and applying any CPU fallback, before calling a GGML or llama function.
 *
 * @param directory Selected runtime directory, including its dependencies.
 * @return The first failed DLL and Win32 error, or std::nullopt on success.
 */
std::optional<LoadError> preload(const std::filesystem::path& directory);

} // namespace WindowsGgmlRuntime
