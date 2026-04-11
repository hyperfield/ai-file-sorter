#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDesktopServices>
#include <QUrl>
#include <QByteArray>
#include <QObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStringList>

#include "UpdaterLaunchOptions.hpp"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <utility>

#include <windows.h>
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif
using SetProcessDpiAwarenessContextFn = BOOL (WINAPI *)(HANDLE);
using SetProcessDpiAwarenessFn = HRESULT (WINAPI *)(int); // 2 = PROCESS_PER_MONITOR_DPI_AWARE

#ifndef AIFS_MAIN_EXECUTABLE_NAME
#define AIFS_MAIN_EXECUTABLE_NAME "aifilesorter.exe"
#endif

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

QStringList candidateGgmlDirectories(const QString& exeDir, const QString& variant)
{
    QStringList candidates;
    candidates << QDir(exeDir).filePath(QStringLiteral("lib/ggml/%1").arg(variant));
    candidates << QDir(exeDir).filePath(QStringLiteral("ggml/%1").arg(variant));
    return candidates;
}

QStringList requiredGgmlPayloadFiles(BackendSelection selection)
{
    QStringList required = {
        QStringLiteral("llama.dll"),
        QStringLiteral("ggml.dll"),
    };
    switch (selection) {
        case BackendSelection::Cuda:
            required << QStringLiteral("ggml-cuda.dll");
            break;
        case BackendSelection::Vulkan:
            required << QStringLiteral("ggml-vulkan.dll");
            break;
        case BackendSelection::Cpu:
        default:
            break;
    }
    return required;
}

bool hasRequiredGgmlPayload(const QString& directory, BackendSelection selection)
{
    const QDir dir(directory);
    if (!dir.exists()) {
        return false;
    }

    for (const QString& filename : requiredGgmlPayloadFiles(selection)) {
        if (!QFileInfo::exists(dir.filePath(filename))) {
            return false;
        }
    }
    return true;
}

QString pickCudaProbeDirectory(const QString& exeDir, bool* payloadPresent = nullptr)
{
    const QStringList candidates = candidateGgmlDirectories(exeDir, QStringLiteral("wcuda"));
    for (const QString& candidate : candidates) {
        if (hasRequiredGgmlPayload(candidate, BackendSelection::Cuda)) {
            if (payloadPresent) {
                *payloadPresent = true;
            }
            return candidate;
        }
    }

    for (const QString& candidate : candidates) {
        if (QDir(candidate).exists()) {
            if (payloadPresent) {
                *payloadPresent = false;
            }
            return candidate;
        }
    }

    if (payloadPresent) {
        *payloadPresent = false;
    }
    return QString();
}

void appendUniqueDirectory(QStringList& directories, const QString& candidate)
{
    if (candidate.isEmpty()) {
        return;
    }

    const QString normalized = QDir::cleanPath(QDir(candidate).absolutePath());
    if (!QDir(normalized).exists()) {
        return;
    }

    for (const QString& existing : directories) {
        if (QString::compare(existing, normalized, Qt::CaseInsensitive) == 0) {
            return;
        }
    }

    directories.append(normalized);
}

void addCudaRootCandidates(QStringList& directories, const QString& root)
{
    if (root.isEmpty()) {
        return;
    }

    appendUniqueDirectory(directories, QDir(root).filePath(QStringLiteral("bin/x64")));
    appendUniqueDirectory(directories, QDir(root).filePath(QStringLiteral("bin")));
    appendUniqueDirectory(directories, root);
}

QStringList candidateCudaRuntimeDirectories()
{
    QStringList directories;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    addCudaRootCandidates(directories, env.value(QStringLiteral("CUDA_PATH")));

    const QStringList keys = env.keys();
    for (const QString& key : keys) {
        if (key.startsWith(QStringLiteral("CUDA_PATH_V"), Qt::CaseInsensitive)) {
            addCudaRootCandidates(directories, env.value(key));
        }
    }

    const QDir toolkitRoot(QStringLiteral("C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA"));
    if (toolkitRoot.exists()) {
        const QFileInfoList entries = toolkitRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                                QDir::Name | QDir::Reversed);
        for (const QFileInfo& entry : entries) {
            addCudaRootCandidates(directories, entry.absoluteFilePath());
        }
    }

    return directories;
}

int parseCudaRuntimeVersionToken(const QString& fileName)
{
    static const QRegularExpression runtimePattern(
        QStringLiteral("^cudart64_(\\d+)\\.dll$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = runtimePattern.match(fileName);
    if (!match.hasMatch()) {
        return 0;
    }
    return match.captured(1).toInt();
}

struct CudaRuntimeDetection {
    bool driverPresent{false};
    bool runtimePresent{false};
    QString runtimeLibraryPath;
    int runtimeVersionToken{0};
};

bool isNvidiaDriverPresent()
{
    const QString systemRoot = qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows"));
    return QFileInfo::exists(QDir(systemRoot).filePath(QStringLiteral("System32/nvcuda.dll")));
}

CudaRuntimeDetection detectCudaRuntime()
{
    CudaRuntimeDetection detection;
    detection.driverPresent = isNvidiaDriverPresent();
    const auto chooseBestRuntime = [](const QStringList& directories) -> std::pair<QString, int> {
        QString bestPath;
        int bestVersion = 0;
        bool bestIsX64 = false;

        for (const QString& directory : directories) {
            const QDir dir(directory);
            const QFileInfoList candidates = dir.entryInfoList({QStringLiteral("cudart64_*.dll")},
                                                               QDir::Files,
                                                               QDir::Name);
            for (const QFileInfo& candidate : candidates) {
                const int version = parseCudaRuntimeVersionToken(candidate.fileName());
                if (version <= 0) {
                    continue;
                }

                const QString absolutePath = QDir::cleanPath(candidate.absoluteFilePath());
                const bool isX64 =
                    absolutePath.contains(QStringLiteral("\\bin\\x64"), Qt::CaseInsensitive) ||
                    absolutePath.contains(QStringLiteral("/bin/x64"), Qt::CaseInsensitive);
                const bool isToolkitPath =
                    absolutePath.contains(QStringLiteral("NVIDIA GPU Computing Toolkit\\CUDA"), Qt::CaseInsensitive) ||
                    absolutePath.contains(QStringLiteral("NVIDIA GPU Computing Toolkit/CUDA"), Qt::CaseInsensitive);
                const bool bestIsToolkitPath =
                    bestPath.contains(QStringLiteral("NVIDIA GPU Computing Toolkit\\CUDA"), Qt::CaseInsensitive) ||
                    bestPath.contains(QStringLiteral("NVIDIA GPU Computing Toolkit/CUDA"), Qt::CaseInsensitive);
                if ((isToolkitPath && !bestIsToolkitPath) ||
                    (isToolkitPath == bestIsToolkitPath && version > bestVersion) ||
                    (version == bestVersion && isToolkitPath == bestIsToolkitPath && isX64 && !bestIsX64) ||
                    (version == bestVersion && isX64 == bestIsX64 &&
                     (bestPath.isEmpty() || QString::compare(absolutePath, bestPath, Qt::CaseInsensitive) < 0))) {
                    bestPath = absolutePath;
                    bestVersion = version;
                    bestIsX64 = isX64;
                }
            }
        }

        return {bestPath, bestVersion};
    };

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList preferredDirectories;
    addCudaRootCandidates(preferredDirectories, env.value(QStringLiteral("CUDA_PATH")));
    auto preferredRuntime = chooseBestRuntime(preferredDirectories);
    QString bestPath = preferredRuntime.first;
    int bestVersion = preferredRuntime.second;
    if (bestPath.isEmpty()) {
        const auto fallbackRuntime = chooseBestRuntime(candidateCudaRuntimeDirectories());
        bestPath = fallbackRuntime.first;
        bestVersion = fallbackRuntime.second;
    }

    if (!bestPath.isEmpty()) {
        detection.runtimePresent = true;
        detection.runtimeLibraryPath = bestPath;
        detection.runtimeVersionToken = bestVersion;
    }

    return detection;
}

QString cudaRuntimeName(const CudaRuntimeDetection& detection)
{
    if (detection.runtimeLibraryPath.isEmpty()) {
        return QString();
    }
    return QFileInfo(detection.runtimeLibraryPath).fileName();
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
        QObject::tr("CUDA Runtime Missing or Incompatible"),
        QObject::tr("A compatible NVIDIA GPU was detected, but the required CUDA runtime for the bundled CUDA backend could not be found or initialized.\n\n"
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
                          const QString& llamaDevice,
                          const QProcessEnvironment& extraEnvironment) {
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
    for (const QString& key : extraEnvironment.keys()) {
        environment.insert(key, extraEnvironment.value(key));
    }

    QProcess process;
    process.setProcessEnvironment(environment);
    process.setProgram(executablePath);
    process.setArguments(arguments);
    process.setWorkingDirectory(exeInfo.absolutePath());

    return process.startDetached();
}

QString resolveExecutableName(const QString& baseDir, const QString& currentExecutablePath) {
    const QStringList candidates = {
        QStringLiteral(AIFS_MAIN_EXECUTABLE_NAME),
        QStringLiteral("aifilesorter-bin.exe"),
        QStringLiteral("aifilesorter.exe"),
        QStringLiteral("AI File Sorter.exe")
    };

    const QString currentAbsolutePath = QFileInfo(currentExecutablePath).absoluteFilePath();

    for (const QString& candidate : candidates) {
        const QString fullPath = QDir(baseDir).filePath(candidate);
        if (!QFileInfo::exists(fullPath)) {
            continue;
        }
        if (QString::compare(QFileInfo(fullPath).absoluteFilePath(),
                             currentAbsolutePath,
                             Qt::CaseInsensitive) == 0) {
            continue;
        }
        return fullPath;
    }

    return QDir(baseDir).filePath(candidates.front());
}

struct BackendOverrides {
    BackendOverride cuda{BackendOverride::None};
    BackendOverride vulkan{BackendOverride::None};
    bool blindCuda{false};
    QStringList observedArgs;
};

constexpr auto kDevBlindCudaFlag = "--dev-blind-cuda";

std::optional<bool> parse_developer_bool_argument(const QString& argument,
                                                  const QString& flag_name)
{
    if (argument == flag_name) {
        return true;
    }

    const QString prefix = flag_name + QStringLiteral("=");
    if (!argument.startsWith(prefix)) {
        return std::nullopt;
    }

    const QString value = argument.mid(prefix.size()).trimmed().toLower();
    if (value.isEmpty() ||
        value == QLatin1String("1") ||
        value == QLatin1String("true") ||
        value == QLatin1String("on") ||
        value == QLatin1String("yes")) {
        return true;
    }
    if (value == QLatin1String("0") ||
        value == QLatin1String("false") ||
        value == QLatin1String("off") ||
        value == QLatin1String("no")) {
        return false;
    }

    qWarning().noquote()
        << "Ignoring invalid value for"
        << flag_name
        << ":"
        << argument;
    return false;
}

bool is_bootstrapper_only_argument(const QString& argument)
{
    return argument == QLatin1String(kDevBlindCudaFlag) ||
           argument.startsWith(QString::fromLatin1(kDevBlindCudaFlag) + QStringLiteral("="));
}

struct UpdaterLiveTestArgs {
    bool enabled{false};
    QString installerUrl;
    QString installerSha256;
    QString currentVersion;
    QString minVersion;
};

struct BackendAvailability {
    bool hasNvidiaDriver{false};
    bool cudaPayloadPresent{false};
    bool cudaRuntimeDetected{false};
    bool runtimeCompatible{false};
    bool cudaBackendLoadable{false};
    bool cudaAvailable{false};
    bool vulkanAvailable{false};
    bool cudaInitiallyAvailable{false};
    bool vulkanInitiallyAvailable{false};
    QString detectedCudaRuntime;
    QString detectedCudaRuntimeDirectory;
    QString cudaFailureReason;
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
        } else if (const auto blind_cuda = parse_developer_bool_argument(
                       arg,
                       QString::fromLatin1(kDevBlindCudaFlag))) {
            overrides.blindCuda = *blind_cuda;
        }
    }
    return overrides;
}

bool consume_flag_value(const QString& argument, const char* prefix, QString& target)
{
    const QString prefix_text = QString::fromLatin1(prefix);
    if (!argument.startsWith(prefix_text)) {
        return false;
    }
    target = argument.mid(prefix_text.size());
    return true;
}

UpdaterLiveTestArgs parse_updater_live_test_args(int argc, char* argv[])
{
    UpdaterLiveTestArgs args;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == QLatin1String(UpdaterLaunchOptions::kLiveTestFlag)) {
            args.enabled = true;
            continue;
        }
        if (consume_flag_value(argument, UpdaterLaunchOptions::kLiveTestUrlFlag, args.installerUrl)) {
            continue;
        }
        if (consume_flag_value(argument, UpdaterLaunchOptions::kLiveTestSha256Flag, args.installerSha256)) {
            continue;
        }
        if (consume_flag_value(argument, UpdaterLaunchOptions::kLiveTestVersionFlag, args.currentVersion)) {
            continue;
        }
        if (consume_flag_value(argument, UpdaterLaunchOptions::kLiveTestMinVersionFlag, args.minVersion)) {
            continue;
        }
    }
    return args;
}

QProcessEnvironment build_updater_live_test_environment(const UpdaterLiveTestArgs& args)
{
    QProcessEnvironment environment;
    if (!args.enabled) {
        return environment;
    }

    environment.insert(QString::fromLatin1(UpdaterLaunchOptions::kLiveTestModeEnv), QStringLiteral("1"));
    if (!args.installerUrl.isEmpty()) {
        environment.insert(QString::fromLatin1(UpdaterLaunchOptions::kLiveTestUrlEnv), args.installerUrl);
    }
    if (!args.installerSha256.isEmpty()) {
        environment.insert(QString::fromLatin1(UpdaterLaunchOptions::kLiveTestSha256Env), args.installerSha256);
    }
    if (!args.currentVersion.isEmpty()) {
        environment.insert(QString::fromLatin1(UpdaterLaunchOptions::kLiveTestVersionEnv), args.currentVersion);
    }
    if (!args.minVersion.isEmpty()) {
        environment.insert(QString::fromLatin1(UpdaterLaunchOptions::kLiveTestMinVersionEnv), args.minVersion);
    }
    return environment;
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
    if (!availability.cudaPayloadPresent) {
        return false;
    }

    const bool runtimeMissing = !availability.cudaRuntimeDetected;
    const bool runtimeIncompatible =
        availability.cudaRuntimeDetected &&
        (!availability.runtimeCompatible || !availability.cudaBackendLoadable);
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
    if (overrides.cuda == BackendOverride::ForceOn && overrides.blindCuda) {
        QMessageBox::critical(nullptr,
                              QObject::tr("Launch Error"),
                              QObject::tr("Cannot force CUDA while %1 is active.")
                                  .arg(QString::fromLatin1(kDevBlindCudaFlag)));
        return false;
    }
    return true;
}

BackendAvailability detect_backend_availability(const QString& exeDir,
                                                const CudaRuntimeDetection& cudaDetection,
                                                bool cudaPayloadPresent)
{
    BackendAvailability availability;
    availability.hasNvidiaDriver = cudaDetection.driverPresent;
    availability.cudaPayloadPresent = cudaPayloadPresent;
    availability.cudaRuntimeDetected = cudaDetection.runtimePresent;
    availability.runtimeCompatible = cudaDetection.runtimePresent;
    availability.cudaBackendLoadable = cudaPayloadPresent;
    availability.detectedCudaRuntime = cudaRuntimeName(cudaDetection);
    availability.detectedCudaRuntimeDirectory = cudaDetection.runtimeLibraryPath.isEmpty()
        ? QString()
        : QFileInfo(cudaDetection.runtimeLibraryPath).absolutePath();
    availability.cudaAvailable =
        availability.hasNvidiaDriver &&
        availability.cudaRuntimeDetected &&
        availability.cudaPayloadPresent;
    availability.vulkanAvailable = isVulkanRuntimeAvailable(exeDir);
    availability.cudaInitiallyAvailable = availability.cudaAvailable;
    availability.vulkanInitiallyAvailable = availability.vulkanAvailable;

    if (availability.hasNvidiaDriver && availability.cudaRuntimeDetected && !availability.runtimeCompatible) {
        availability.cudaFailureReason = QStringLiteral("runtime-unusable");
    } else if (availability.hasNvidiaDriver && availability.runtimeCompatible && !availability.cudaBackendLoadable) {
        availability.cudaFailureReason = availability.cudaPayloadPresent
            ? QStringLiteral("backend-mismatch")
            : QStringLiteral("backend-missing");
        qWarning().noquote()
            << "Detected CUDA runtime"
            << (availability.detectedCudaRuntime.isEmpty() ? QStringLiteral("<unknown>") : availability.detectedCudaRuntime)
            << "but the bundled ggml-cuda backend could not be loaded."
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
    if (availability.cudaAvailable) {
        return BackendSelection::Cuda;
    }
    if (availability.vulkanAvailable) {
        return BackendSelection::Vulkan;
    }
    return selection;
}

QString incompatible_runtime_message(const BackendAvailability& availability)
{
    if (availability.cudaRuntimeDetected && !availability.runtimeCompatible) {
        return QStringLiteral("CUDA runtime ignored due to incompatibility; using CPU backend.");
    }
    if (availability.cudaRuntimeDetected && availability.runtimeCompatible && !availability.cudaBackendLoadable) {
        return availability.cudaPayloadPresent
            ? QStringLiteral("CUDA runtime detected, but the packaged ggml-cuda backend could not be loaded; using CPU backend.")
            : QStringLiteral("CUDA runtime detected, but this build does not include a packaged CUDA backend; using CPU backend.");
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

void enable_per_monitor_dpi_awareness()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        const auto set_ctx = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_ctx && set_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        const auto set_awareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
            GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (set_awareness) {
            set_awareness(2); // PROCESS_PER_MONITOR_DPI_AWARE
        }
        FreeLibrary(shcore);
    }
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
            qInfo().noquote() << "Backend selection: Vulkan (CUDA unavailable; priority order CUDA → Vulkan → CPU).";
            break;
        case BackendSelection::Cuda:
            qInfo().noquote() << "Backend selection: CUDA (priority order CUDA → Vulkan → CPU).";
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
                               BackendSelection selection,
                               bool showError = true)
{
    const QStringList candidates = candidateGgmlDirectories(exeDir, variant);
    for (const QString& candidate : candidates) {
        if (hasRequiredGgmlPayload(candidate, selection)) {
            if (candidate != candidates.front()) {
                qInfo().noquote() << "Primary GGML directory missing; using fallback"
                                  << QDir::toNativeSeparators(candidate);
            }
            return candidate;
        }
        if (QDir(candidate).exists()) {
            qWarning().noquote()
                << "Ignoring GGML directory without expected payload:"
                << QDir::toNativeSeparators(candidate);
        }
    }

    if (showError) {
        QMessageBox::critical(
            nullptr,
            QObject::tr("Missing GGML Runtime"),
            QObject::tr("Could not locate usable backend runtime DLLs.\nTried:\n%1\n%2")
                .arg(QDir::toNativeSeparators(candidates.value(0)),
                     QDir::toNativeSeparators(candidates.value(1))));
    }
    return QString();
}

void configure_runtime_paths(const QString& exeDir,
                             const QString& ggmlPath,
                             bool secureSearchEnabled,
                             bool useCuda,
                             bool useVulkan,
                             const QString& cudaRuntimeDir = QString())
{
    appendToProcessPath(ggmlPath);
    if (secureSearchEnabled) {
        addDllDirectoryChecked(ggmlPath);
    }

    QStringList additionalDllRoots;
    additionalDllRoots << QDir(exeDir).filePath(QStringLiteral("lib/precompiled/cpu/bin"));
    if (useCuda) {
        if (!cudaRuntimeDir.isEmpty()) {
            additionalDllRoots << cudaRuntimeDir;
        }
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
        if (is_bootstrapper_only_argument(arg)) {
            continue;
        }
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
                         const QString& ggmlPath,
                         const UpdaterLiveTestArgs& updaterLiveTest)
{
    const bool disableCudaEnv = (selection != BackendSelection::Cuda);
    const QString backendTag = backend_tag_for_selection(selection);
    const QString llamaDevice = llama_device_for_selection(selection);
    if (!launchMainExecutable(mainExecutable,
                              forwardedArgs,
                              disableCudaEnv,
                              backendTag,
                              ggmlPath,
                              llamaDevice,
                              build_updater_live_test_environment(updaterLiveTest))) {
        QMessageBox::critical(nullptr,
            QObject::tr("Launch Failed"),
            QObject::tr("Failed to launch the main application executable:\n%1").arg(mainExecutable));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    enable_per_monitor_dpi_awareness();
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    const QString exeDir = QCoreApplication::applicationDirPath();
    QDir::setCurrent(exeDir);

    const bool secureSearchEnabled = enableSecureDllSearch();
    if (!secureSearchEnabled) {
        qWarning() << "SetDefaultDllDirectories unavailable; relying on PATH order for DLL resolution.";
    }

    BackendOverrides overrides = parse_backend_overrides(argc, argv);
    const UpdaterLiveTestArgs updaterLiveTest = parse_updater_live_test_args(argc, argv);
    log_observed_arguments(overrides.observedArgs);
    if (!validate_override_conflict(overrides)) {
        return EXIT_FAILURE;
    }

    bool cudaPayloadPresent = false;
    CudaRuntimeDetection cudaDetection;
    if (overrides.blindCuda) {
        qInfo().noquote()
            << "Developer override active:"
            << QString::fromLatin1(kDevBlindCudaFlag)
            << "- skipping CUDA detection.";
    } else {
        const QString cudaProbeDir = pickCudaProbeDirectory(exeDir, &cudaPayloadPresent);
        if (!cudaProbeDir.isEmpty()) {
            qInfo().noquote()
                << "Detected packaged CUDA runtime payload at"
                << QDir::toNativeSeparators(cudaProbeDir);
        }
        cudaDetection = detectCudaRuntime();
    }

    BackendAvailability availability = detect_backend_availability(exeDir,
                                                                   cudaDetection,
                                                                   cudaPayloadPresent);
    apply_override_flags(overrides, availability);
    if (maybe_prompt_cuda_download(overrides, availability)) {
        return EXIT_SUCCESS;
    }
    BackendSelection selection = resolve_backend_selection(overrides, availability);

    QString ggmlVariant = ggml_variant_for_selection(selection);
    QString ggmlPath = resolve_ggml_directory(exeDir, ggmlVariant, selection, /*showError=*/false);
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

        ggmlPath = resolve_ggml_directory(exeDir, ggmlVariant, selection, /*showError=*/true);
        if (ggmlPath.isEmpty()) {
            return EXIT_FAILURE;
        }
    }

    log_runtime_availability(availability, selection);

    const bool useCuda = (selection == BackendSelection::Cuda);
    const bool useVulkan = (selection == BackendSelection::Vulkan);
    configure_runtime_paths(exeDir,
                            ggmlPath,
                            secureSearchEnabled,
                            useCuda,
                            useVulkan,
                            availability.detectedCudaRuntimeDirectory);

    bool console_log_flag = false;
    QStringList forwardedArgs = build_forwarded_args(argc, argv, console_log_flag);
    if (console_log_flag) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$", "r", stdin);
    }

    const QString mainExecutable = resolveExecutableName(
        exeDir,
        QCoreApplication::applicationFilePath());
    if (!launch_main_process(mainExecutable, forwardedArgs, selection, ggmlPath, updaterLiveTest)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
