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

enum class BackendSelection {
    Cpu,
    Cuda,
    Vulkan
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

QStringList candidateGgmlDirectories(const QString& exeDir, const QString& variant)
{
    QStringList candidates;
    candidates << QDir(exeDir).filePath(QStringLiteral("lib/ggml/%1").arg(variant));
    candidates << QDir(exeDir).filePath(QStringLiteral("ggml/%1").arg(variant));
    return candidates;
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

bool isRequiredCudaRuntimePresent(QString *loadedRuntime = nullptr) {
    static const QList<int> requiredVersions = { 13 }; // keep in sync with build script
    for (int version : requiredVersions) {
        const QString runtime = QStringLiteral("cudart64_%1").arg(version);
        if (tryLoadLibrary(runtime)) {
            if (loadedRuntime) {
                *loadedRuntime = runtime;
            }
            return true;
        }
    }
    return false;
}

bool loadVulkanLibrary(const QString& path) {
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    HMODULE module = LoadLibraryW(native.c_str());
    if (!module) {
        return false;
    }
    FreeLibrary(module);
    return true;
}

bool isVulkanRuntimeAvailable(const QString& exeDir) {
    if (loadVulkanLibrary(QStringLiteral("vulkan-1.dll"))) {
        qInfo().noquote() << "Detected system Vulkan runtime via PATH.";
        return true;
    }

    const QStringList bundledCandidates = {
        QDir(exeDir).filePath(QStringLiteral("lib/precompiled/vulkan/bin/vulkan-1.dll")),
    };

    QStringList ggmlCandidates = candidateGgmlDirectories(exeDir, QStringLiteral("wvulkan"));
    for (QString& root : ggmlCandidates) {
        root = QDir(root).filePath(QStringLiteral("vulkan-1.dll"));
    }

    for (const QString& candidate : bundledCandidates + ggmlCandidates) {
        if (QFileInfo::exists(candidate)) {
            qInfo().noquote()
                << "Detected bundled Vulkan runtime at"
                << QDir::toNativeSeparators(candidate);
            return true;
        }
    }

    return false;
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
    process.setProgram(executablePath);
    process.setArguments(arguments);
    process.setWorkingDirectory(exeInfo.absolutePath());

    return process.startDetached();
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

    const bool cudaRuntimeDetected = isCudaAvailable();
    const bool hasNvidiaDriver = isNvidiaDriverAvailable();

    if (hasNvidiaDriver && !cudaRuntimeDetected) {
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
    QStringList observedArgs;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        observedArgs << arg;
        if (arg.startsWith("--cuda=")) {
            cudaOverride = parseBackendOverride(arg.mid(7));
        } else if (arg.startsWith("--vulkan=")) {
            vulkanOverride = parseBackendOverride(arg.mid(9));
        }
    }

    if (!observedArgs.isEmpty()) {
        const QString argLine = observedArgs.join(QLatin1Char(' '));
        qInfo().noquote() << "Starter arguments:" << argLine;
    }

    if (cudaOverride == BackendOverride::ForceOn && vulkanOverride == BackendOverride::ForceOn) {
        QMessageBox::critical(nullptr, QObject::tr("Launch Error"), QObject::tr("Cannot enable both CUDA and Vulkan simultaneously."));
        return EXIT_FAILURE;
    }

    QString detectedCudaRuntime;
    const bool runtimeCompatible = isRequiredCudaRuntimePresent(&detectedCudaRuntime);
    if (hasNvidiaDriver && cudaRuntimeDetected && !runtimeCompatible) {
        qWarning().noquote()
            << "Detected CUDA runtime" << detectedCudaRuntime
            << "but the bundled GGML build requires cudart64_13.dll. Falling back to alternate backend.";
    }

    bool cudaAvailable = runtimeCompatible && hasNvidiaDriver;
    bool vulkanAvailable = isVulkanRuntimeAvailable(exeDir);
    const bool cudaInitiallyAvailable = cudaAvailable;
    const bool vulkanInitiallyAvailable = vulkanAvailable;

    const QString availabilityLine =
        QStringLiteral("Runtime availability: CUDA=%1 Vulkan=%2")
            .arg(cudaInitiallyAvailable ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(vulkanInitiallyAvailable ? QStringLiteral("yes") : QStringLiteral("no"));
    qInfo().noquote() << availabilityLine;

    const bool cudaForcedOn = (cudaOverride == BackendOverride::ForceOn);
    const bool cudaForcedOff = (cudaOverride == BackendOverride::ForceOff);
    const bool vulkanForcedOn = (vulkanOverride == BackendOverride::ForceOn);
    const bool vulkanForcedOff = (vulkanOverride == BackendOverride::ForceOff);

    if (cudaForcedOff) {
        cudaAvailable = false;
        qInfo().noquote() << "CUDA manually disabled via --cuda=off.";
    }
    if (vulkanForcedOff) {
        vulkanAvailable = false;
        qInfo().noquote() << "Vulkan manually disabled via --vulkan=off.";
    }

    BackendSelection selection = BackendSelection::Cpu;
    bool selectionDecided = false;

    if (vulkanForcedOn) {
        if (vulkanAvailable) {
            selection = BackendSelection::Vulkan;
            selectionDecided = true;
        } else {
            qWarning().noquote() << "Vulkan forced but not detected; ignoring request.";
        }
    }

    if (!selectionDecided && cudaForcedOn) {
        if (cudaAvailable) {
            selection = BackendSelection::Cuda;
            selectionDecided = true;
        } else {
            qWarning().noquote() << "CUDA forced but not detected; ignoring request.";
        }
    }

    if (!selectionDecided) {
        if (vulkanAvailable) {
            selection = BackendSelection::Vulkan;
            selectionDecided = true;
        } else if (cudaAvailable) {
            selection = BackendSelection::Cuda;
            selectionDecided = true;
        }
    }

    const bool useCuda = (selection == BackendSelection::Cuda);
    const bool useVulkan = (selection == BackendSelection::Vulkan);

    if (!useCuda && !useVulkan) {
        if (cudaForcedOff && vulkanForcedOff) {
            qInfo().noquote() << "CUDA and Vulkan explicitly disabled; using CPU backend.";
        } else if (vulkanInitiallyAvailable && !vulkanAvailable) {
            qInfo().noquote() << "Vulkan runtime ignored due to override; using CPU backend.";
        } else if (cudaInitiallyAvailable && !cudaAvailable) {
            qInfo().noquote() << "CUDA runtime ignored due to override; using CPU backend.";
        } else {
            qInfo().noquote() << "No GPU runtime detected; using CPU backend.";
        }
    } else if (useVulkan) {
        qInfo().noquote() << "Backend selection: Vulkan (priority order Vulkan → CUDA → CPU).";
    } else if (useCuda) {
        qInfo().noquote() << "Backend selection: CUDA (Vulkan unavailable).";
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

    const QStringList ggmlCandidates = candidateGgmlDirectories(exeDir, ggmlVariant);

    QString ggmlPath;
    for (const QString& candidate : ggmlCandidates) {
        if (QDir(candidate).exists()) {
            ggmlPath = candidate;
            if (candidate != ggmlCandidates.front()) {
                qInfo().noquote() << "Primary GGML directory missing; using fallback"
                                  << QDir::toNativeSeparators(candidate);
            }
            break;
        }
    }

    if (ggmlPath.isEmpty()) {
        QMessageBox::critical(
            nullptr,
            QObject::tr("Missing GGML Runtime"),
            QObject::tr("Could not locate the backend runtime DLLs.\nTried:\n%1\n%2")
                .arg(QDir::toNativeSeparators(ggmlCandidates.value(0)),
                     QDir::toNativeSeparators(ggmlCandidates.value(1))));
        return EXIT_FAILURE;
    }

    appendToProcessPath(ggmlPath);
    if (secureSearchEnabled) {
        addDllDirectoryChecked(ggmlPath);
    }

    // Include other runtime directories so Qt/OpenSSL/etc can be found.
    QStringList additionalDllRoots;
    additionalDllRoots << QDir(exeDir).filePath(QStringLiteral("lib/precompiled/cpu/bin"));
    if (useCuda) {
        additionalDllRoots << QDir(exeDir).filePath(QStringLiteral("lib/precompiled/cuda/bin"));
    }
    if (useVulkan) {
        additionalDllRoots << QDir(exeDir).filePath(QStringLiteral("lib/precompiled/vulkan/bin"));
    }
    additionalDllRoots << QDir(exeDir).filePath(QStringLiteral("bin"));
    additionalDllRoots << exeDir;
    for (const QString& dir : additionalDllRoots) {
        if (!QDir(dir).exists()) {
            continue;
        }
        appendToProcessPath(dir);
        if (secureSearchEnabled) {
            addDllDirectoryChecked(dir);
        }
    }

    QStringList forwardedArgs;
    for (int i = 1; i < argc; ++i) {
        forwardedArgs.append(QString::fromLocal8Bit(argv[i]));
    }
    forwardedArgs.prepend(QStringLiteral("--allow-direct-launch"));

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



