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

struct BackendOverrides {
    BackendOverride cuda{BackendOverride::None};
    BackendOverride vulkan{BackendOverride::None};
    QStringList observedArgs;
};

struct BackendAvailability {
    bool hasNvidiaDriver{false};
    bool cudaRuntimeDetected{false};
    bool runtimeCompatible{false};
    bool cudaAvailable{false};
    bool vulkanAvailable{false};
    bool cudaInitiallyAvailable{false};
    bool vulkanInitiallyAvailable{false};
    QString detectedCudaRuntime;
};

BackendOverrides parse_backend_overrides(int argc, char* argv[])
{
    BackendOverrides overrides;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        overrides.observedArgs << arg;
        if (arg.startsWith(QStringLiteral("--cuda="))) {
            overrides.cuda = parseBackendOverride(arg.mid(7));
        } else if (arg.startsWith(QStringLiteral("--vulkan="))) {
            overrides.vulkan = parseBackendOverride(arg.mid(9));
        }
    }
    return overrides;
}

void log_observed_arguments(const QStringList& args)
{
    if (args.isEmpty()) {
        return;
    }
    qInfo().noquote() << "Starter arguments:" << args.join(QLatin1Char(' '));
}

bool maybe_prompt_cuda_download(bool hasNvidiaDriver, bool cudaRuntimeDetected)
{
    if (hasNvidiaDriver && !cudaRuntimeDetected) {
        return promptCudaDownload();
    }
    return false;
}

bool validate_override_conflict(const BackendOverrides& overrides)
{
    if (overrides.cuda == BackendOverride::ForceOn &&
        overrides.vulkan == BackendOverride::ForceOn) {
        QMessageBox::critical(nullptr,
                              QObject::tr("Launch Error"),
                              QObject::tr("Cannot enable both CUDA and Vulkan simultaneously."));
        return false;
    }
    return true;
}

BackendAvailability detect_backend_availability(const QString& exeDir,
                                                bool hasNvidiaDriver,
                                                bool cudaRuntimeDetected)
{
    BackendAvailability availability;
    availability.hasNvidiaDriver = hasNvidiaDriver;
    availability.cudaRuntimeDetected = cudaRuntimeDetected;
    availability.runtimeCompatible = isRequiredCudaRuntimePresent(&availability.detectedCudaRuntime);
    availability.cudaAvailable = availability.runtimeCompatible && hasNvidiaDriver;
    availability.vulkanAvailable = isVulkanRuntimeAvailable(exeDir);
    availability.cudaInitiallyAvailable = availability.cudaAvailable;
    availability.vulkanInitiallyAvailable = availability.vulkanAvailable;
    if (hasNvidiaDriver && cudaRuntimeDetected && !availability.runtimeCompatible) {
        qWarning().noquote()
            << "Detected CUDA runtime" << availability.detectedCudaRuntime
            << "but the bundled GGML build requires cudart64_13.dll. Falling back to alternate backend.";
    }
    return availability;
}

void apply_override_flags(const BackendOverrides& overrides,
                          BackendAvailability& availability)
{
    if (overrides.cuda == BackendOverride::ForceOff) {
        availability.cudaAvailable = false;
        qInfo().noquote() << "CUDA manually disabled via --cuda=off.";
    }
    if (overrides.vulkan == BackendOverride::ForceOff) {
        availability.vulkanAvailable = false;
        qInfo().noquote() << "Vulkan manually disabled via --vulkan=off.";
    }
}

BackendSelection resolve_backend_selection(const BackendOverrides& overrides,
                                           const BackendAvailability& availability)
{
    BackendSelection selection = BackendSelection::Cpu;
    if (overrides.vulkan == BackendOverride::ForceOn) {
        if (availability.vulkanAvailable) {
            return BackendSelection::Vulkan;
        }
        qWarning().noquote() << "Vulkan forced but not detected; ignoring request.";
    }
    if (overrides.cuda == BackendOverride::ForceOn) {
        if (availability.cudaAvailable) {
            return BackendSelection::Cuda;
        }
        qWarning().noquote() << "CUDA forced but not detected; ignoring request.";
    }
    if (availability.vulkanAvailable) {
        return BackendSelection::Vulkan;
    }
    if (availability.cudaAvailable) {
        return BackendSelection::Cuda;
    }
    return selection;
}

void log_runtime_availability(const BackendAvailability& availability,
                              BackendSelection selection)
{
    const QString availabilityLine =
        QStringLiteral("Runtime availability: CUDA=%1 Vulkan=%2")
            .arg(availability.cudaInitiallyAvailable ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(availability.vulkanInitiallyAvailable ? QStringLiteral("yes") : QStringLiteral("no"));
    qInfo().noquote() << availabilityLine;

    if (selection == BackendSelection::Vulkan) {
        qInfo().noquote() << "Backend selection: Vulkan (priority order Vulkan → CUDA → CPU).";
    } else if (selection == BackendSelection::Cuda) {
        qInfo().noquote() << "Backend selection: CUDA (Vulkan unavailable).";
    } else {
        if (!availability.cudaAvailable && !availability.vulkanAvailable) {
            if (availability.cudaRuntimeDetected && !availability.runtimeCompatible) {
                qInfo().noquote() << "CUDA runtime ignored due to incompatibility; using CPU backend.";
            } else {
                qInfo().noquote() << "No GPU runtime detected; using CPU backend.";
            }
        } else if (availability.cudaInitiallyAvailable && !availability.cudaAvailable) {
            qInfo().noquote() << "CUDA runtime ignored due to override; using CPU backend.";
        } else if (availability.vulkanInitiallyAvailable && !availability.vulkanAvailable) {
            qInfo().noquote() << "Vulkan runtime ignored due to override; using CPU backend.";
        } else {
            qInfo().noquote() << "CUDA and Vulkan explicitly disabled; using CPU backend.";
        }
    }
}

QString ggml_variant_for_selection(BackendSelection selection)
{
    switch (selection) {
        case BackendSelection::Cuda:
            return QStringLiteral("wcuda");
        case BackendSelection::Vulkan:
            return QStringLiteral("wvulkan");
        case BackendSelection::Cpu:
        default:
            return QStringLiteral("wocuda");
    }
}

QString resolve_ggml_directory(const QString& exeDir, const QString& variant)
{
    const QStringList candidates = candidateGgmlDirectories(exeDir, variant);
    for (const QString& candidate : candidates) {
        if (QDir(candidate).exists()) {
            if (candidate != candidates.front()) {
                qInfo().noquote() << "Primary GGML directory missing; using fallback"
                                  << QDir::toNativeSeparators(candidate);
            }
            return candidate;
        }
    }

    QMessageBox::critical(
        nullptr,
        QObject::tr("Missing GGML Runtime"),
        QObject::tr("Could not locate the backend runtime DLLs.\nTried:\n%1\n%2")
            .arg(QDir::toNativeSeparators(candidates.value(0)),
                 QDir::toNativeSeparators(candidates.value(1))));
    return QString();
}

void configure_runtime_paths(const QString& exeDir,
                             const QString& ggmlPath,
                             bool secureSearchEnabled,
                             bool useCuda,
                             bool useVulkan)
{
    appendToProcessPath(ggmlPath);
    if (secureSearchEnabled) {
        addDllDirectoryChecked(ggmlPath);
    }

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
}

QStringList build_forwarded_args(int argc, char* argv[])
{
    QStringList forwardedArgs;
    for (int i = 1; i < argc; ++i) {
        forwardedArgs.append(QString::fromLocal8Bit(argv[i]));
    }
    forwardedArgs.prepend(QStringLiteral("--allow-direct-launch"));
    return forwardedArgs;
}

QString backend_tag_for_selection(BackendSelection selection)
{
    switch (selection) {
        case BackendSelection::Cuda: return QStringLiteral("cuda");
        case BackendSelection::Vulkan: return QStringLiteral("vulkan");
        case BackendSelection::Cpu:
        default: return QStringLiteral("cpu");
    }
}

QString llama_device_for_selection(BackendSelection selection)
{
    switch (selection) {
        case BackendSelection::Cuda: return QStringLiteral("cuda");
        case BackendSelection::Vulkan: return QStringLiteral("vulkan");
        case BackendSelection::Cpu:
        default: return QString();
    }
}

bool launch_main_process(const QString& mainExecutable,
                         const QStringList& forwardedArgs,
                         BackendSelection selection,
                         const QString& ggmlPath)
{
    const bool disableCudaEnv = (selection != BackendSelection::Cuda);
    const QString backendTag = backend_tag_for_selection(selection);
    const QString llamaDevice = llama_device_for_selection(selection);
    if (!launchMainExecutable(mainExecutable,
                              forwardedArgs,
                              disableCudaEnv,
                              backendTag,
                              ggmlPath,
                              llamaDevice)) {
        QMessageBox::critical(nullptr,
            QObject::tr("Launch Failed"),
            QObject::tr("Failed to launch the main application executable:\n%1").arg(mainExecutable));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    const QString exeDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(exeDir);

    const bool cudaRuntimeDetected = isCudaAvailable();
    const bool hasNvidiaDriver = isNvidiaDriverAvailable();

    if (maybe_prompt_cuda_download(hasNvidiaDriver, cudaRuntimeDetected)) {
        return EXIT_SUCCESS;
    }

    const bool secureSearchEnabled = enableSecureDllSearch();
    if (!secureSearchEnabled) {
        qWarning() << "SetDefaultDllDirectories unavailable; relying on PATH order for DLL resolution.";
    }

    BackendOverrides overrides = parse_backend_overrides(argc, argv);
    log_observed_arguments(overrides.observedArgs);
    if (!validate_override_conflict(overrides)) {
        return EXIT_FAILURE;
    }

    BackendAvailability availability = detect_backend_availability(exeDir, hasNvidiaDriver, cudaRuntimeDetected);
    apply_override_flags(overrides, availability);
    const BackendSelection selection = resolve_backend_selection(overrides, availability);
    log_runtime_availability(availability, selection);

    const QString ggmlVariant = ggml_variant_for_selection(selection);
    const QString ggmlPath = resolve_ggml_directory(exeDir, ggmlVariant);
    if (ggmlPath.isEmpty()) {
        return EXIT_FAILURE;
    }

    const bool useCuda = (selection == BackendSelection::Cuda);
    const bool useVulkan = (selection == BackendSelection::Vulkan);
    configure_runtime_paths(exeDir, ggmlPath, secureSearchEnabled, useCuda, useVulkan);

    const QString mainExecutable = resolveExecutableName(exeDir);
    const QStringList forwardedArgs = build_forwarded_args(argc, argv);
    if (!launch_main_process(mainExecutable, forwardedArgs, selection, ggmlPath)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


