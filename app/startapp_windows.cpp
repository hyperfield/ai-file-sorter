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

const QList<int>& knownCudaRuntimeVersions()
{
    static const QList<int> versions = {
        75, 80, 90, 91, 92,      // CUDA 7.5–9.2
        100, 101, 102,           // CUDA 10.x
        110, 111, 112, 113, 114, 115, 116, 117, 118, // CUDA 11.x variants
        120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130 // CUDA 12.x variants
    };
    return versions;
}

const QList<int>& requiredCudaRuntimeVersions()
{
    static const QList<int> versions = { 120 }; // keep in sync with build script (CUDA 12.x)
    return versions;
}

bool isCudaRuntimePresent(const QList<int>& versions, QString *loadedRuntime = nullptr)
{
    for (int version : versions) {
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

bool isCudaAvailable(QString *loadedRuntime = nullptr) {
    return isCudaRuntimePresent(knownCudaRuntimeVersions(), loadedRuntime);
}

bool isRequiredCudaRuntimePresent(QString *loadedRuntime = nullptr) {
    return isCudaRuntimePresent(requiredCudaRuntimeVersions(), loadedRuntime);
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

bool maybe_prompt_cuda_download(const BackendOverrides& overrides,
                                const BackendAvailability& availability)
{
    if (!availability.hasNvidiaDriver) {
        return false;
    }

    const bool runtimeMissing = !availability.cudaRuntimeDetected;
    const bool runtimeIncompatible = availability.cudaRuntimeDetected && !availability.runtimeCompatible;
    if (!runtimeMissing && !runtimeIncompatible) {
        return false;
    }
    if (overrides.cuda == BackendOverride::ForceOff) {
        return false;
    }

    const bool cudaRequested = overrides.cuda == BackendOverride::ForceOn;
    const bool vulkanUnavailable = !availability.vulkanAvailable;
    if (!cudaRequested && !vulkanUnavailable) {
        return false;
    }

    return promptCudaDownload();
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
                                                bool cudaRuntimeDetected,
                                                const QString& detectedRuntimeName)
{
    BackendAvailability availability;
    availability.hasNvidiaDriver = hasNvidiaDriver;
    availability.cudaRuntimeDetected = cudaRuntimeDetected;
    QString compatibleRuntime;
    availability.runtimeCompatible = isRequiredCudaRuntimePresent(&compatibleRuntime);
    availability.detectedCudaRuntime = availability.runtimeCompatible ? compatibleRuntime : detectedRuntimeName;
    availability.cudaAvailable = availability.runtimeCompatible && hasNvidiaDriver;
    availability.vulkanAvailable = isVulkanRuntimeAvailable(exeDir);
    availability.cudaInitiallyAvailable = availability.cudaAvailable;
    availability.vulkanInitiallyAvailable = availability.vulkanAvailable;
    if (hasNvidiaDriver && cudaRuntimeDetected && !availability.runtimeCompatible) {
        const QString requiredRuntime = QStringLiteral("cudart64_%1.dll").arg(requiredCudaRuntimeVersions().constFirst());
        qWarning().noquote()
            << "Detected CUDA runtime"
            << (availability.detectedCudaRuntime.isEmpty() ? QStringLiteral("<unknown>") : availability.detectedCudaRuntime)
            << "but the bundled GGML build requires"
            << requiredRuntime << "."
            << "Falling back to alternate backend.";
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

QString incompatible_runtime_message(const BackendAvailability& availability)
{
    if (availability.cudaRuntimeDetected && !availability.runtimeCompatible) {
        return QStringLiteral("CUDA runtime ignored due to incompatibility; using CPU backend.");
    }
    return QStringLiteral("No GPU runtime detected; using CPU backend.");
}

QString cpu_backend_message(const BackendAvailability& availability)
{
    if (!availability.cudaAvailable && !availability.vulkanAvailable) {
        return incompatible_runtime_message(availability);
    }
    if (availability.cudaInitiallyAvailable && !availability.cudaAvailable) {
        return QStringLiteral("CUDA runtime ignored due to override; using CPU backend.");
    }
    if (availability.vulkanInitiallyAvailable && !availability.vulkanAvailable) {
        return QStringLiteral("Vulkan runtime ignored due to override; using CPU backend.");
    }
    return QStringLiteral("CUDA and Vulkan explicitly disabled; using CPU backend.");
}

void log_runtime_availability(const BackendAvailability& availability,
                              BackendSelection selection)
{
    const QString availabilityLine =
        QStringLiteral("Runtime availability: CUDA=%1 Vulkan=%2")
            .arg(availability.cudaInitiallyAvailable ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(availability.vulkanInitiallyAvailable ? QStringLiteral("yes") : QStringLiteral("no"));
    qInfo().noquote() << availabilityLine;

    switch (selection) {
        case BackendSelection::Vulkan:
            qInfo().noquote() << "Backend selection: Vulkan (priority order Vulkan → CUDA → CPU).";
            break;
        case BackendSelection::Cuda:
            qInfo().noquote() << "Backend selection: CUDA (Vulkan unavailable).";
            break;
        case BackendSelection::Cpu:
        default:
            qInfo().noquote() << cpu_backend_message(availability);
            break;
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

QString resolve_ggml_directory(const QString& exeDir,
                               const QString& variant,
                               bool showError = true)
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

    if (showError) {
        QMessageBox::critical(
            nullptr,
            QObject::tr("Missing GGML Runtime"),
            QObject::tr("Could not locate the backend runtime DLLs.\nTried:\n%1\n%2")
                .arg(QDir::toNativeSeparators(candidates.value(0)),
                     QDir::toNativeSeparators(candidates.value(1))));
    }
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

QStringList build_forwarded_args(int argc, char* argv[], bool &console_log_flag)
{
    QStringList forwardedArgs;
    console_log_flag = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--console-log")) {
            console_log_flag = true;
        }
        forwardedArgs.append(arg);
    }
    forwardedArgs.prepend(QStringLiteral("--allow-direct-launch"));
    if (console_log_flag && !forwardedArgs.contains(QStringLiteral("--console-log"))) {
        forwardedArgs.append(QStringLiteral("--console-log"));
    }
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

    QString detectedCudaRuntime;
    const bool cudaRuntimeDetected = isCudaAvailable(&detectedCudaRuntime);
    const bool hasNvidiaDriver = isNvidiaDriverAvailable();

    const bool secureSearchEnabled = enableSecureDllSearch();
    if (!secureSearchEnabled) {
        qWarning() << "SetDefaultDllDirectories unavailable; relying on PATH order for DLL resolution.";
    }

    BackendOverrides overrides = parse_backend_overrides(argc, argv);
    log_observed_arguments(overrides.observedArgs);
    if (!validate_override_conflict(overrides)) {
        return EXIT_FAILURE;
    }

    BackendAvailability availability = detect_backend_availability(exeDir,
                                                                   hasNvidiaDriver,
                                                                   cudaRuntimeDetected,
                                                                   detectedCudaRuntime);
    apply_override_flags(overrides, availability);
    if (maybe_prompt_cuda_download(overrides, availability)) {
        return EXIT_SUCCESS;
    }
    BackendSelection selection = resolve_backend_selection(overrides, availability);

    QString ggmlVariant = ggml_variant_for_selection(selection);
    QString ggmlPath = resolve_ggml_directory(exeDir, ggmlVariant, /*showError=*/false);
    if (ggmlPath.isEmpty()) {
        qWarning().noquote()
            << "Backend runtime directory missing for selection" << ggmlVariant
            << "- attempting fallback.";

        BackendSelection fallbackSelection = BackendSelection::Cpu;
        if (selection == BackendSelection::Vulkan && availability.cudaAvailable) {
            fallbackSelection = BackendSelection::Cuda;
        } else if (selection == BackendSelection::Cuda && availability.vulkanAvailable) {
            fallbackSelection = BackendSelection::Vulkan;
        }

        if (fallbackSelection != selection) {
            qInfo().noquote()
                << "Falling back to backend"
                << backend_tag_for_selection(fallbackSelection)
                << "due to missing runtime directory.";
            selection = fallbackSelection;
            ggmlVariant = ggml_variant_for_selection(selection);
        } else {
            qInfo().noquote() << "Falling back to CPU backend.";
            selection = BackendSelection::Cpu;
            ggmlVariant = ggml_variant_for_selection(selection);
        }

        ggmlPath = resolve_ggml_directory(exeDir, ggmlVariant, /*showError=*/true);
        if (ggmlPath.isEmpty()) {
            return EXIT_FAILURE;
        }
    }

    log_runtime_availability(availability, selection);

    const bool useCuda = (selection == BackendSelection::Cuda);
    const bool useVulkan = (selection == BackendSelection::Vulkan);
    configure_runtime_paths(exeDir, ggmlPath, secureSearchEnabled, useCuda, useVulkan);

    bool console_log_flag = false;
    QStringList forwardedArgs = build_forwarded_args(argc, argv, console_log_flag);
    if (console_log_flag) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$", "r", stdin);
    }

    const QString mainExecutable = resolveExecutableName(exeDir);
    if (!launch_main_process(mainExecutable, forwardedArgs, selection, ggmlPath)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
