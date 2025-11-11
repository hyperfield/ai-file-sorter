#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <string>
#include <sys/stat.h>
#include <vector>
#include <dlfcn.h>

std::string getExecutableDirectory() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    std::string path(result, (count > 0) ? count : 0);
    size_t pos = path.find_last_of("/\\");
    return path.substr(0, pos);
}


bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}


void addToLdLibraryPath(const std::string& dir) {
    const char* oldPath = getenv("LD_LIBRARY_PATH");
    std::string newPath = dir;
    if (oldPath) {
        newPath = std::string(oldPath) + ":" + dir;
    }
    setenv("LD_LIBRARY_PATH", newPath.c_str(), 1);
}


bool isCudaInstalled() {
    return system("ldconfig -p | grep -q libcudart") == 0;
}

bool isVulkanAvailable() {
    void* handle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!handle) {
        return false;
    }
    dlclose(handle);
    return true;
}


extern char **environ;

std::vector<std::string> collect_environment_variables()
{
    std::vector<std::string> envVars;
    for (char **env = environ; *env != nullptr; ++env) {
        envVars.emplace_back(*env);
    }
    return envVars;
}

void ensure_executable(const std::string& exePath)
{
    if (access(exePath.c_str(), X_OK) != 0) {
        std::fprintf(stderr, "App is not executable: %s\n", exePath.c_str());
        perror("access");
        exit(EXIT_FAILURE);
    }
}

void set_or_append_env(std::vector<std::string>& envVars,
                       const std::string& prefix,
                       const std::string& value)
{
    for (auto& env : envVars) {
        if (env.rfind(prefix, 0) == 0) {
            env = prefix + value;
            return;
        }
    }
    envVars.push_back(prefix + value);
}

std::vector<char*> build_envp(std::vector<std::string>& envVars)
{
    std::vector<char*> envp;
    envp.reserve(envVars.size() + 1);
    for (auto &s : envVars) {
        envp.push_back(s.data());
    }
    envp.push_back(nullptr);
    return envp;
}

std::vector<char*> build_argv(const std::string& exePath, int argc, char** argv)
{
    std::vector<std::string> arg_storage;
    arg_storage.push_back(exePath);
    for (int i = 1; i < argc; ++i) {
        if (argv[i]) {
            arg_storage.emplace_back(argv[i]);
        }
    }

    std::vector<char*> argv_ptrs;
    argv_ptrs.reserve(arg_storage.size() + 1);
    for (auto& arg : arg_storage) {
        argv_ptrs.push_back(arg.data());
    }
    argv_ptrs.push_back(nullptr);
    return argv_ptrs;
}

void launch_with_env(const std::string& exePath,
                     std::vector<char*>& argv_ptrs,
                     std::vector<char*>& envp)
{
    execve(exePath.c_str(), argv_ptrs.data(), envp.data());
    std::fprintf(stderr, "execve failed\n");
    perror("execve failed");
    exit(EXIT_FAILURE);
}

void launchMainApp(const std::string& exeDir,
                   const std::string& libPath,
                   int argc,
                   char** argv,
                   bool disable_cuda,
                   const std::string& backend_tag,
                   const std::string& ggml_dir,
                   const std::string& llama_device) {
    const std::string exePath = exeDir + "/bin/aifilesorter";
    ensure_executable(exePath);

    std::vector<std::string> envVars = collect_environment_variables();
    set_or_append_env(envVars, "LD_LIBRARY_PATH=", libPath);
    set_or_append_env(envVars, "GGML_DISABLE_CUDA=", disable_cuda ? "1" : "0");
    set_or_append_env(envVars, "AI_FILE_SORTER_GPU_BACKEND=", backend_tag);
    set_or_append_env(envVars, "AI_FILE_SORTER_GGML_DIR=", ggml_dir);
    set_or_append_env(envVars, "LLAMA_ARG_DEVICE=", llama_device);

    std::vector<char*> envp = build_envp(envVars);
    std::vector<char*> argv_ptrs = build_argv(exePath, argc, argv);
    launch_with_env(exePath, argv_ptrs, envp);
}

void launchMainApp(const std::string& exeDir,
                   const std::string& libPath,
                   int argc,
                   char** argv,
                   bool disable_cuda,
                   const std::string& backend_tag,
                   const std::string& ggml_dir,
                   const std::string& llama_device);


struct BackendOverrideFlags {
    std::optional<bool> cuda;
    std::optional<bool> vulkan;
};

BackendOverrideFlags parse_backend_overrides(int argc, char* argv[])
{
    BackendOverrideFlags overrides;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg.rfind("--cuda=", 0) == 0) {
            overrides.cuda = (arg.substr(7) == "on");
        } else if (arg.rfind("--vulkan=", 0) == 0) {
            overrides.vulkan = (arg.substr(9) == "on");
        }
    }
    return overrides;
}

bool validate_overrides(const BackendOverrideFlags& overrides)
{
    if (overrides.cuda.has_value() && overrides.vulkan.has_value() &&
        overrides.cuda.value() && overrides.vulkan.value()) {
        std::cerr << "Cannot force both CUDA and Vulkan simultaneously." << std::endl;
        return false;
    }
    return true;
}

struct BackendState {
    bool cuda_available{false};
    bool vulkan_available{false};
    BackendSelection selection{BackendSelection::Cpu};
    std::string ggml_subdir;
    std::string backend_tag{"cpu"};
    std::string llama_device;
};

BackendState decide_backend(const BackendOverrideFlags& overrides,
                            const std::string& baseLibDir)
{
    BackendState state;
    state.cuda_available = isCudaInstalled();
    state.vulkan_available = isVulkanAvailable();

    bool useCuda = state.cuda_available;
    bool useVulkan = !useCuda && state.vulkan_available;

    if (overrides.cuda.has_value()) {
        useCuda = overrides.cuda.value();
        if (useCuda && !state.cuda_available) {
            std::cerr << "Warning: CUDA forced but not detected; falling back." << std::endl;
            useCuda = false;
        }
    }
    if (overrides.vulkan.has_value()) {
        useVulkan = overrides.vulkan.value();
        if (useVulkan && !state.vulkan_available) {
            std::cerr << "Warning: Vulkan forced but not detected; falling back." << std::endl;
            useVulkan = false;
        }
    }
    if (useCuda && useVulkan) {
        useVulkan = false; // CUDA has priority
    }

    if (useCuda) {
        state.selection = BackendSelection::Cuda;
        state.ggml_subdir = baseLibDir + "/ggml/wcuda";
        state.backend_tag = "cuda";
        state.llama_device = "cuda";
        std::cout << "Using CUDA backend." << std::endl;
    } else if (useVulkan) {
        state.selection = BackendSelection::Vulkan;
        state.ggml_subdir = baseLibDir + "/ggml/wvulkan";
        state.backend_tag = "vulkan";
        state.llama_device = "vulkan";
        std::cout << "Using Vulkan backend." << std::endl;
    } else {
        state.selection = BackendSelection::Cpu;
        state.ggml_subdir = baseLibDir + "/ggml/wocuda";
        state.backend_tag = "cpu";
        state.llama_device.clear();
        std::cout << "Using CPU backend." << std::endl;
    }

    return state;
}

int main(int argc, char* argv[]) {
    const std::string exeDir = getExecutableDirectory();
    const std::string baseLibDir = exeDir + "/lib";

    BackendOverrideFlags overrides = parse_backend_overrides(argc, argv);
    if (!validate_overrides(overrides)) {
        return EXIT_FAILURE;
    }

    BackendState backend = decide_backend(overrides, baseLibDir);

    const std::string fullLdPath = backend.ggml_subdir + ":" + baseLibDir;
    const bool disableCudaEnv = (backend.selection != BackendSelection::Cuda);
    launchMainApp(exeDir,
                  fullLdPath,
                  argc,
                  argv,
                  disableCudaEnv,
                  QString::fromStdString(backend.backend_tag),
                  QString::fromStdString(backend.ggml_subdir),
                  QString::fromStdString(backend.llama_device));
    return EXIT_SUCCESS;
}
