#include "WindowsGgmlRuntime.hpp"

#ifdef _WIN32
#include <windows.h>

#include <array>

namespace {

class ScopedLoaderErrorMode {
public:
    ScopedLoaderErrorMode()
        : active_(SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previous_) != 0)
    {
    }

    ~ScopedLoaderErrorMode()
    {
        if (active_) {
            SetThreadErrorMode(previous_, nullptr);
        }
    }

    ScopedLoaderErrorMode(const ScopedLoaderErrorMode&) = delete;
    ScopedLoaderErrorMode& operator=(const ScopedLoaderErrorMode&) = delete;

private:
    DWORD previous_{0};
    bool active_;
};

} // namespace

std::optional<WindowsGgmlRuntime::LoadError> WindowsGgmlRuntime::preload(
    const std::filesystem::path& directory)
{
    constexpr std::array runtime_libraries = {
        L"ggml-base.dll",
        L"ggml-cpu.dll",
        L"ggml.dll",
        L"llama.dll",
        L"llama-common.dll",
#ifdef AI_FILE_SORTER_HAS_MTMD
        L"mtmd.dll",
#endif
    };
    const auto absolute_directory = std::filesystem::absolute(directory);
    const ScopedLoaderErrorMode suppress_loader_dialogs;
    for (const auto* filename : runtime_libraries) {
        const auto library_path = absolute_directory / filename;
        // Keep the module reference until process exit so delay-load imports
        // reuse these modules instead of performing another DLL path search.
        if (!LoadLibraryExW(library_path.c_str(), nullptr,
                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
            return LoadError{library_path, GetLastError()};
        }
    }
    return std::nullopt;
}
#endif
