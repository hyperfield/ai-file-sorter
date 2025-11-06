#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <string>
#include <sys/stat.h>
#include <vector>

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


extern char **environ;

void launchMainApp(const std::string& exeDir, const std::string& libPath, int argc, char** argv) {
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

    if (isCudaInstalled()) {
        ggmlSubdir = baseLibDir + "/ggml/wcuda";
        std::cout << "CUDA detected. Using CUDA libraries." << std::endl;
    } else {
        ggmlSubdir = baseLibDir + "/ggml/wocuda";
        std::cout << "No CUDA detected. Using non-CUDA libraries." << std::endl;
    }

    std::string fullLdPath = ggmlSubdir + ":" + baseLibDir;
    launchMainApp(exeDir, fullLdPath, argc, argv);
    return 0;
}
