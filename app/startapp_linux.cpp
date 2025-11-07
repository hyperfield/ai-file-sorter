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

void launchMainApp(const std::string& exeDir,
                   const std::string& libPath,
                   int argc,
                   char** argv,
                   bool disable_cuda,
                   const std::string& backend_tag,
                   const std::string& ggml_dir,
                   const std::string& llama_device) {
    std::string exePath = exeDir + "/bin/aifilesorter";

    if (access(exePath.c_str(), X_OK) != 0) {
        std::fprintf(stderr, "App is not executable: %s\n", exePath.c_str());
        perror("access");
        exit(EXIT_FAILURE);
    }

    // Copy current environment
    std::vector<std::string> envVars;
    for (char **env = environ; *env != nullptr; ++env) {
        envVars.emplace_back(*env);
    }

    // Overwrite or append LD_LIBRARY_PATH
    bool foundLd = false;
    for (auto &env : envVars) {
        if (env.find("LD_LIBRARY_PATH=") == 0) {
            env = "LD_LIBRARY_PATH=" + libPath;
            foundLd = true;
            break;
        }
    }
    if (!foundLd) {
        envVars.push_back("LD_LIBRARY_PATH=" + libPath);
    }

    const std::string cuda_prefix = "GGML_DISABLE_CUDA=";
    bool cuda_entry_found = false;
    for (auto& env : envVars) {
        if (env.rfind(cuda_prefix, 0) == 0) {
            env = cuda_prefix + (disable_cuda ? "1" : "0");
            cuda_entry_found = true;
            break;
        }
    }
    if (!cuda_entry_found) {
        envVars.push_back(cuda_prefix + (disable_cuda ? "1" : "0"));
    }

    const std::string backend_prefix = "AI_FILE_SORTER_GPU_BACKEND=";
    bool backend_entry_found = false;
    for (auto& env : envVars) {
        if (env.rfind(backend_prefix, 0) == 0) {
            env = backend_prefix + backend_tag;
            backend_entry_found = true;
            break;
        }
    }
    if (!backend_entry_found) {
        envVars.push_back(backend_prefix + backend_tag);
    }

    const std::string ggml_prefix = "AI_FILE_SORTER_GGML_DIR=";
    bool ggml_entry_found = false;
    for (auto& env : envVars) {
        if (env.rfind(ggml_prefix, 0) == 0) {
            env = ggml_prefix + ggml_dir;
            ggml_entry_found = true;
            break;
        }
    }
    if (!ggml_entry_found) {
        envVars.push_back(ggml_prefix + ggml_dir);
    }

    const std::string llama_prefix = "LLAMA_ARG_DEVICE=";
    bool llama_entry_found = false;
    for (auto& env : envVars) {
        if (env.rfind(llama_prefix, 0) == 0) {
            env = llama_prefix + llama_device;
            llama_entry_found = true;
            break;
        }
    }
    if (!llama_entry_found) {
        envVars.push_back(llama_prefix + llama_device);
    }

    // Convert to char*[]
    std::vector<char*> envp;
    for (auto &s : envVars) {
        envp.push_back(&s[0]);  // get pointer to internal buffer
    }
    envp.push_back(nullptr);

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

    execve(exePath.c_str(), argv_ptrs.data(), envp.data());

    std::fprintf(stderr, "execve failed\n");
    perror("execve failed");
    exit(EXIT_FAILURE);
}


int main(int argc, char* argv[]) {
    std::string exeDir = getExecutableDirectory();
    std::string baseLibDir = exeDir + "/lib";
    std::string ggmlSubdir;

    std::string cudaOverride;
    std::string vulkanOverride;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg.rfind("--cuda=", 0) == 0) {
            cudaOverride = arg.substr(7);
        } else if (arg.rfind("--vulkan=", 0) == 0) {
            vulkanOverride = arg.substr(9);
        }
    }

    auto parse_override = [](const std::string& value, bool& out) {
        if (value == "on") {
            out = true;
            return true;
        }
        if (value == "off") {
            out = false;
            return true;
        }
        return false;
    };

    bool forceCuda;
    bool hasCudaOverride = parse_override(cudaOverride, forceCuda);
    bool forceVulkan;
    bool hasVulkanOverride = parse_override(vulkanOverride, forceVulkan);

    if (hasCudaOverride && hasVulkanOverride && forceCuda && forceVulkan) {
        std::cerr << "Cannot force both CUDA and Vulkan simultaneously." << std::endl;
        return 1;
    }

    bool cudaAvailable = isCudaInstalled();
    bool vulkanAvailable = isVulkanAvailable();

    bool useCuda = cudaAvailable;
    bool useVulkan = !useCuda && vulkanAvailable;

    if (hasCudaOverride) {
        useCuda = forceCuda;
        if (forceCuda && !cudaAvailable) {
            std::cerr << "Warning: CUDA forced but not detected; falling back." << std::endl;
            useCuda = false;
        }
    }
    if (hasVulkanOverride) {
        useVulkan = forceVulkan;
        if (forceVulkan && !vulkanAvailable) {
            std::cerr << "Warning: Vulkan forced but not detected; falling back." << std::endl;
            useVulkan = false;
        }
    }
    if (useCuda && useVulkan) {
        useVulkan = false; // CUDA has priority
    }

    std::string backend_tag = "cpu";
    std::string llamaDeviceValue;
    if (useCuda) {
        ggmlSubdir = baseLibDir + "/ggml/wcuda";
        std::cout << "Using CUDA backend." << std::endl;
        backend_tag = "cuda";
        llamaDeviceValue = "cuda";
    } else if (useVulkan) {
        ggmlSubdir = baseLibDir + "/ggml/wvulkan";
        std::cout << "Using Vulkan backend." << std::endl;
        backend_tag = "vulkan";
        llamaDeviceValue = "vulkan";
    } else {
        ggmlSubdir = baseLibDir + "/ggml/wocuda";
        std::cout << "Using CPU backend." << std::endl;
        llamaDeviceValue.clear();
    }

    std::string fullLdPath = ggmlSubdir + ":" + baseLibDir;
    const bool disableCudaEnv = !useCuda;
    launchMainApp(exeDir, fullLdPath, argc, argv, disableCudaEnv, backend_tag, ggmlSubdir, llamaDeviceValue);
    return 0;
}
