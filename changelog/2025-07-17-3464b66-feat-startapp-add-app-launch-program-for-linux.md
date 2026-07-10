# 2025-07-17: feat(startapp): add app launch program for Linux

## Covered commits
- `3464b66` `2025-07-17` `feat(startapp): add app launch program for Linux`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `app/startapp_linux.cpp`
- `A` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/startapp_linux.cpp`, `app/startapp_windows.cpp`. It changed the project from not having the capability described by `feat(startapp): add app launch program for Linux` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `3464b66`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/startapp_linux.cpp b/app/startapp_linux.cpp
--- /dev/null
+++ b/app/startapp_linux.cpp
@@ -0,0 +1,103 @@
+#include <iostream>
+#include <cstdlib>
+#include <unistd.h>
+#include <limits.h>
+#include <string>
+#include <sys/stat.h>
+#include <vector>
+
+
+std::string getExecutableDirectory() {
+    char result[PATH_MAX];
+    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
+    std::string path(result, (count > 0) ? count : 0);
+    size_t pos = path.find_last_of("/\\");
+    return path.substr(0, pos);
+}
+
+
+bool fileExists(const std::string& path) {
+    struct stat buffer;
+    return (stat(path.c_str(), &buffer) == 0);
+}
+
+
+void addToLdLibraryPath(const std::string& dir) {
+    const char* oldPath = getenv("LD_LIBRARY_PATH");
+    std::string newPath = dir;
+    if (oldPath) {
+        newPath = std::string(oldPath) + ":" + dir;
+    }
+    setenv("LD_LIBRARY_PATH", newPath.c_str(), 1);
+}
+
+
+bool isCudaInstalled() {
+    return system("ldconfig -p | grep -q libcudart") == 0;
+}
+
+
+extern char **environ;
+
+void launchMainApp(const std::string& exeDir, const std::string& libPath) {
+    std::string exePath = exeDir + "/bin/aifilesorter";
+
+    if (access(exePath.c_str(), X_OK) != 0) {
+        std::cerr << "App is not executable: " << exePath << std::endl;
+        perror("access");
+        exit(EXIT_FAILURE);
+    }
+
+    // Copy current environment
+    std::vector<std::string> envVars;
+    for (char **env = environ; *env != nullptr; ++env) {
+        envVars.emplace_back(*env);
+    }
+
+    // Overwrite or append LD_LIBRARY_PATH
+    bool foundLd = false;
+    for (auto &env : envVars) {
+        if (env.find("LD_LIBRARY_PATH=") == 0) {
+            env = "LD_LIBRARY_PATH=" + libPath;
+            foundLd = true;
+            break;
+        }
+    }
+    if (!foundLd) {
+        envVars.push_back("LD_LIBRARY_PATH=" + libPath);
+    }
+
+    // Convert to char*[]
+    std::vector<char*> envp;
+    for (auto &s : envVars) {
+        envp.push_back(&s[0]);  // get pointer to internal buffer
+    }
+    envp.push_back(nullptr);
+
+    // Args
+    const char* argv[] = { exePath.c_str(), nullptr };
+
+    execve(exePath.c_str(), const_cast<char* const*>(argv), envp.data());
+
+    perror("execve failed");
+    exit(EXIT_FAILURE);
+}
+
+
+int main() {
+    std::string exeDir = getExecutableDirectory();
+    std::string baseLibDir = exeDir + "/lib";
+    std::string ggmlSubdir;
+
+    if (isCudaInstalled()) {
+        ggmlSubdir = baseLibDir + "/ggml/wcuda";
+        std::cout << "CUDA detected. Using CUDA libraries." << std::endl;
+    } else {
+        ggmlSubdir = baseLibDir + "/ggml/wocuda";
```

The excerpt is taken from the commit diff for `feat(startapp): add app launch program for Linux`. The most relevant surfaces are `app/startapp_linux.cpp`, `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
