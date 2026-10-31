#include "ImageAnalyzerFactory.hpp"

#include "GgufFileValidation.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include <QProcess>
#include <QProcessEnvironment>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string_view>
#include <stdexcept>

namespace {

std::filesystem::path require_artifact_path(const VisualLlmRuntime::Backend& backend,
                                            VisualModelArtifactKind kind,
                                            const char* label)
{
    const auto path = backend.path_for(kind);
    if (!path.has_value()) {
        throw std::runtime_error(std::string("Visual backend is missing required artifact: ") + label);
    }
    return *path;
}

void validate_visual_artifact_file(const std::filesystem::path& path, const char* label)
{
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(std::string("Visual ") + label +
                                 " artifact could not be opened: " +
                                 Utils::path_to_utf8(path));
    }

    if (!has_gguf_header(path)) {
        throw std::runtime_error(std::string("Visual ") + label +
                                 " artifact is invalid or incomplete (expected GGUF header): " +
                                 Utils::path_to_utf8(path));
    }
}

#if defined(_WIN32) || defined(__APPLE__)
std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::optional<bool> read_env_bool(const char* key)
{
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }

    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<int> read_env_int(const char* key)
{
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }

    char* end_ptr = nullptr;
    const long parsed = std::strtol(value, &end_ptr, 10);
    if (end_ptr == value || (end_ptr && *end_ptr != '\0')) {
        return std::nullopt;
    }
    if (parsed <= 0 || parsed > 300000) {
        return std::nullopt;
    }
    return static_cast<int>(parsed);
}

bool should_skip_visual_gpu_preflight()
{
    if (const auto enabled = read_env_bool("AI_FILE_SORTER_VISUAL_GPU_PREFLIGHT")) {
        return !*enabled;
    }
    if (const auto skip = read_env_bool("AI_FILE_SORTER_VISUAL_SKIP_GPU_PREFLIGHT")) {
        return *skip;
    }
    return false;
}

int visual_gpu_preflight_timeout_ms()
{
    constexpr int kDefaultTimeoutMs = 30000;
    if (const auto override_value = read_env_int("AI_FILE_SORTER_VISUAL_GPU_PREFLIGHT_TIMEOUT_MS")) {
        return std::clamp(*override_value, 5000, 120000);
    }
    return kDefaultTimeoutMs;
}

QProcessEnvironment visual_gpu_preflight_environment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("AI_FILE_SORTER_VISUAL_SKIP_GPU_PREFLIGHT"),
                       QStringLiteral("1"));
    environment.insert(QStringLiteral("AI_FILE_SORTER_VISUAL_USE_GPU"),
                       QStringLiteral("1"));

    const std::string storage_override = Utils::get_llm_storage_directory_override();
    if (!storage_override.empty()) {
        environment.insert(QStringLiteral("AI_FILE_SORTER_LLM_STORAGE_DIR"),
                           QString::fromStdString(storage_override));
    }

    return environment;
}

QString visual_gpu_probe_argument(const VisualLlmRuntime::Backend& backend)
{
    if (!backend.descriptor || !backend.descriptor->id || backend.descriptor->id[0] == '\0') {
        return QStringLiteral("--visual-gpu-probe");
    }
    return QStringLiteral("--visual-gpu-probe=%1")
        .arg(QString::fromUtf8(backend.descriptor->id));
}

std::string describe_exit_code(int exit_code)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << static_cast<unsigned int>(exit_code);
    return oss.str();
}

std::string collect_probe_output(QProcess& process)
{
    QString combined = QString::fromLocal8Bit(process.readAllStandardError());
    if (combined.trimmed().isEmpty()) {
        combined = QString::fromLocal8Bit(process.readAllStandardOutput());
    }
    return trim_copy(combined.toStdString());
}

void run_visual_gpu_preflight(const VisualLlmRuntime::Backend& backend, const std::string& required_backend)
{
    auto logger = Logger::get_logger("core_logger");
    const std::string executable_path = Utils::get_executable_path();
    if (executable_path.empty()) {
        if (logger) {
            logger->warn("Skipping visual GPU preflight because the executable path could not be resolved.");
        }
        return;
    }

    const int timeout_ms = visual_gpu_preflight_timeout_ms();
    QProcess process;
    auto environment = visual_gpu_preflight_environment();
    if (!required_backend.empty()) {
        environment.insert(QStringLiteral("AI_FILE_SORTER_VISUAL_REQUIRED_BACKEND"),
                           QString::fromStdString(required_backend));
    }
    process.setProcessEnvironment(environment);
    process.setProgram(QString::fromStdString(executable_path));
    process.setArguments({visual_gpu_probe_argument(backend)});

    if (logger && backend.descriptor) {
        logger->info("Running visual GPU preflight for backend '{}'.", backend.descriptor->id);
    }

    process.start();
    if (!process.waitForStarted(timeout_ms)) {
        throw std::runtime_error("Visual GPU preflight subprocess did not start.");
    }
    if (!process.waitForFinished(timeout_ms)) {
        process.kill();
        process.waitForFinished(5000);
        throw std::runtime_error("Visual GPU preflight timed out.");
    }

    const std::string probe_output = collect_probe_output(process);
    if (process.exitStatus() != QProcess::NormalExit) {
        std::string reason =
            "Visual GPU preflight crashed (exit code " + describe_exit_code(process.exitCode()) + ")";
        if (!probe_output.empty()) {
            reason += ": " + probe_output;
        }
        throw std::runtime_error(reason);
    }
    if (process.exitCode() != 0) {
        if (!probe_output.empty()) {
            throw std::runtime_error(probe_output);
        }
        throw std::runtime_error("Visual GPU preflight failed with exit code " +
                                 std::to_string(process.exitCode()));
    }
}
#endif

} // namespace

#if defined(AI_FILE_SORTER_TEST_BUILD) && (defined(_WIN32) || defined(__APPLE__))
QProcessEnvironment ImageAnalyzerFactoryTestAccess::visual_gpu_preflight_environment()
{
    return ::visual_gpu_preflight_environment();
}
#endif

std::unique_ptr<ImageAnalyzer> ImageAnalyzerFactory::create(const VisualLlmRuntime::Backend& backend,
                                                            ImageAnalyzerSettings settings)
{
    if (!backend.descriptor) {
        throw std::runtime_error("Visual backend descriptor is missing.");
    }

    const auto model_path =
        require_artifact_path(backend, VisualModelArtifactKind::Model, "model");
    const auto mmproj_path =
        require_artifact_path(backend, VisualModelArtifactKind::Mmproj, "mmproj");

    validate_visual_artifact_file(model_path, "model");
    validate_visual_artifact_file(mmproj_path, "multimodal projector");

#if defined(_WIN32) || defined(__APPLE__)
    if (settings.use_gpu && !should_skip_visual_gpu_preflight()) {
        run_visual_gpu_preflight(backend, settings.required_backend);
    }
#endif

    switch (backend.descriptor->architecture) {
    case VisualModelArchitecture::MtmdProjector: {
        return std::make_unique<LlavaImageAnalyzer>(
            model_path,
            mmproj_path,
            backend.descriptor->prompt_policy,
            std::move(settings));
    }
    }

    throw std::runtime_error("Unsupported visual model architecture.");
}
