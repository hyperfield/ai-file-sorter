#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QLibrary>
#include <QDesktopServices>
#include <QUrl>
#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <cstdlib>

#include <windows.h>

namespace {

enum class BackendOverride {
    None,
    ForceOn,
    ForceOff
};

BackendOverride parseBackendOverride(QString value) {
    value = value.trimmed().toLower();
    if (value == QLatin1String("on")) {
        return BackendOverride::ForceOn;
    }
    if (value == QLatin1String("off")) {
        return BackendOverride::ForceOff;
    }
    return BackendOverride::None;
}

bool enableSecureDllSearch()
{
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0602
    return SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) != 0;
#else
    // Only available on Windows 7+ with KB2533623. Try to enable if present.
    typedef BOOL (WINAPI *SetDefaultDllDirectoriesFunc)(DWORD);
    if (const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll")) {
        if (const auto fn = reinterpret_cast<SetDefaultDllDirectoriesFunc>(
                GetProcAddress(kernel32, "SetDefaultDllDirectories"))) {
            return fn(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) != 0;
        }
    }
    return false;
#endif
}

void addDllDirectoryChecked(const QString& directory)
{
    if (directory.isEmpty()) {
        return;
    }
    const std::wstring wideDir = QDir::toNativeSeparators(directory).toStdWString();
    if (AddDllDirectory(wideDir.c_str()) == nullptr) {
        qWarning().noquote()
            << "AddDllDirectory failed for"
            << QDir::toNativeSeparators(directory)
            << "- error" << GetLastError();
    } else {
        qInfo().noquote()
            << "Registered DLL directory"
            << QDir::toNativeSeparators(directory);
    }
}

bool tryLoadLibrary(const QString& name) {
    QLibrary lib(name);
    const bool loaded = lib.load();
    if (loaded) {
        lib.unload();
    }
    return loaded;
}

bool isCudaAvailable() {
    for (int version = 9; version <= 20; ++version) {
        const QString runtime = QStringLiteral("cudart64_%1").arg(version);
        if (tryLoadLibrary(runtime)) {
            return true;
        }
    }
    return false;
}

bool isVulkanAvailable() {
    HMODULE vulkanModule = LoadLibraryW(L"vulkan-1.dll");
    if (!vulkanModule) {
        return false;
    }
    FreeLibrary(vulkanModule);
    return true;
}

bool isNvidiaDriverAvailable() {
    static const QStringList driverCandidates = {
        QStringLiteral("nvml"),
        QStringLiteral("nvcuda"),
        QStringLiteral("nvapi64")
    };

    for (const QString& dll : driverCandidates) {
        if (tryLoadLibrary(dll)) {
            return true;
        }
    }
    return false;
}

void appendToProcessPath(const QString& directory) {
    if (directory.isEmpty()) {
        return;
    }

    QByteArray path = qgetenv("PATH");
    if (!path.isEmpty()) {
        path.append(';');
    }
    path.append(QDir::toNativeSeparators(directory).toUtf8());
    qputenv("PATH", path);
    qInfo().noquote() << "Added to PATH:" << QDir::toNativeSeparators(directory);
    qInfo().noquote() << "Current PATH:" << QString::fromUtf8(qgetenv("PATH"));
}

bool promptCudaDownload() {
    const auto response = QMessageBox::warning(
        nullptr,
        QObject::tr("CUDA Toolkit Missing"),
        QObject::tr("A compatible NVIDIA GPU was detected, but the CUDA Toolkit is missing.\n\n"
                    "CUDA is required for GPU acceleration in this application.\n\n"
                    "Would you like to download and install it now?"),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Ok);

    if (response == QMessageBox::Ok) {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://developer.nvidia.com/cuda-downloads")));
        return true;
    }
    return false;
}

bool launchMainExecutable(const QString& executablePath,
                          const QStringList& arguments,
                          bool disableCuda,
                          const QString& backendTag,
                          const QString& ggmlDir,
                          const QString& llamaDevice) {
    QFileInfo exeInfo(executablePath);
    if (!exeInfo.exists()) {
        return false;
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), QString::fromUtf8(qgetenv("PATH")));
    environment.insert(QStringLiteral("GGML_DISABLE_CUDA"), disableCuda ? QStringLiteral("1") : QStringLiteral("0"));
    environment.insert(QStringLiteral("AI_FILE_SORTER_GPU_BACKEND"), backendTag);
    environment.insert(QStringLiteral("AI_FILE_SORTER_GGML_DIR"), ggmlDir);
    environment.insert(QStringLiteral("LLAMA_ARG_DEVICE"), llamaDevice);

    QProcess process;
    process.setProcessEnvironment(environment);

    return QProcess::startDetached(executablePath, arguments, exeInfo.absolutePath());
}

QString resolveExecutableName(const QString& baseDir) {
    const QStringList candidates = {
        QStringLiteral("aifilesorter.exe"),
        QStringLiteral("AI File Sorter.exe")
    };

    for (const QString& candidate : candidates) {
        const QString fullPath = QDir(baseDir).filePath(candidate);
        if (QFileInfo::exists(fullPath)) {
            return fullPath;
        }
    }

    return QDir(baseDir).filePath(candidates.front());
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    const QString exeDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(exeDir);

    const bool hasCuda = isCudaAvailable();
    const bool hasNvidiaDriver = isNvidiaDriverAvailable();

    if (hasNvidiaDriver && !hasCuda) {
        if (promptCudaDownload()) {
            return EXIT_SUCCESS;
        }
    }

    const bool secureSearchEnabled = enableSecureDllSearch();
    if (!secureSearchEnabled) {
        qWarning() << "SetDefaultDllDirectories unavailable; relying on PATH order for DLL resolution.";
    }

    BackendOverride cudaOverride = BackendOverride::None;
    BackendOverride vulkanOverride = BackendOverride::None;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--cuda=")) {
            cudaOverride = parseBackendOverride(arg.mid(7));
        } else if (arg.startsWith("--vulkan=")) {
            vulkanOverride = parseBackendOverride(arg.mid(9));
        }
    }

    if (cudaOverride == BackendOverride::ForceOn && vulkanOverride == BackendOverride::ForceOn) {
        QMessageBox::critical(nullptr, QObject::tr("Launch Error"), QObject::tr("Cannot enable both CUDA and Vulkan simultaneously."));
        return EXIT_FAILURE;
    }

    bool cudaAvailable = hasCuda && hasNvidiaDriver;
    bool vulkanAvailable = isVulkanAvailable();
    bool useCuda = cudaAvailable;
    bool useVulkan = !useCuda && vulkanAvailable;

    if (cudaOverride == BackendOverride::ForceOn) {
        if (cudaAvailable) {
            useCuda = true;
        } else {
            qWarning().noquote() << "CUDA forced but not detected; falling back.";
            useCuda = false;
        }
    } else if (cudaOverride == BackendOverride::ForceOff) {
        useCuda = false;
    }

    if (vulkanOverride == BackendOverride::ForceOn) {
        if (vulkanAvailable) {
            useVulkan = true;
            if (cudaOverride != BackendOverride::ForceOn) {
                useCuda = false;
            }
        } else {
            qWarning().noquote() << "Vulkan forced but not detected; falling back.";
            useVulkan = false;
        }
    } else if (vulkanOverride == BackendOverride::ForceOff) {
        useVulkan = false;
    }

    if (useCuda && useVulkan) {
        if (cudaOverride == BackendOverride::ForceOn) {
            useVulkan = false;
        } else {
            useCuda = false;
        }
    }

    if (!useCuda && !useVulkan && vulkanAvailable && vulkanOverride != BackendOverride::ForceOff) {
        useVulkan = true;
    }

    QString ggmlVariant;
    if (useCuda) {
        ggmlVariant = QStringLiteral("wcuda");
        qInfo().noquote() << "Using CUDA backend.";
    } else if (useVulkan) {
        ggmlVariant = QStringLiteral("wvulkan");
        qInfo().noquote() << "Using Vulkan backend.";
    } else {
        ggmlVariant = QStringLiteral("wocuda");
        qInfo().noquote() << "Using CPU backend.";
    }
    const QString ggmlPath = QDir(exeDir).filePath(QStringLiteral("lib/ggml/%1").arg(ggmlVariant));
    appendToProcessPath(ggmlPath);
    if (secureSearchEnabled) {
        addDllDirectoryChecked(ggmlPath);
    }

    // Include other runtime directories so Qt/OpenSSL/etc can be found.
    const QStringList additionalDllRoots = {
        QDir(exeDir).filePath(QStringLiteral("lib/precompiled/cpu/bin")),
        QDir(exeDir).filePath(QStringLiteral("lib/precompiled/cuda/bin")),
        QDir(exeDir).filePath(QStringLiteral("lib/precompiled/vulkan/bin")),
        QDir(exeDir).filePath(QStringLiteral("bin")),
        exeDir
    };
    for (const QString& dir : additionalDllRoots) {
        appendToProcessPath(dir);
        if (secureSearchEnabled) {
            addDllDirectoryChecked(dir);
        }
    }

    QStringList forwardedArgs;
    for (int i = 1; i < argc; ++i) {
        forwardedArgs.append(QString::fromLocal8Bit(argv[i]));
    }

    const QString mainExecutable = resolveExecutableName(exeDir);
    const bool disableCudaEnv = !useCuda;
    const QString backendTag = useCuda ? QStringLiteral("cuda")
                                       : (useVulkan ? QStringLiteral("vulkan") : QStringLiteral("cpu"));
    const QString llamaDevice = useCuda ? QStringLiteral("cuda")
                                        : (useVulkan ? QStringLiteral("vulkan") : QString());
    if (!launchMainExecutable(mainExecutable, forwardedArgs, disableCudaEnv, backendTag, ggmlPath, llamaDevice)) {
        QMessageBox::critical(nullptr,
            QObject::tr("Launch Failed"),
            QObject::tr("Failed to launch the main application executable:\n%1").arg(mainExecutable));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
