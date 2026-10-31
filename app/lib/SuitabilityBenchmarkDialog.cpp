#include "SuitabilityBenchmarkDialog.hpp"

#include "DocumentTextAnalyzer.hpp"
#include "GgmlRuntimePaths.hpp"
#include "GgufFileValidation.hpp"
#include "ImageAnalyzerFactory.hpp"
#include "ILLMClient.hpp"
#include "LlmCatalog.hpp"
#include "LocalLLMClient.hpp"
#include "Logger.hpp"
#include "Settings.hpp"
#include "Types.hpp"
#include "Utils.hpp"
#include "VisualLlmRuntime.hpp"
#include "ggml-backend.h"

#include <QComboBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QStandardItemModel>
#include <QDateTime>
#include <QSignalBlocker>
#include <QCloseEvent>
#include <QColor>
#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QOperatingSystemVersion>
#include <QPainter>
#include <QPalette>
#include <QObject>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QRect>
#include <QRegularExpression>
#include <QScrollBar>
#include <QString>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <set>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
enum class PerfClass {
    Optimal,
    Acceptable,
    TooLong
};

struct PerfThresholds {
    double optimal_max{0.0};
    double acceptable_max{0.0};
};

std::string trim_copy(const std::string& value)
{
    std::string trimmed = value;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    return trimmed;
}

std::string format_duration(std::chrono::steady_clock::duration elapsed)
{
    using namespace std::chrono;
    const double seconds = duration_cast<milliseconds>(elapsed).count() / 1000.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << seconds << "s";
    return oss.str();
}

double duration_seconds(std::chrono::steady_clock::duration elapsed)
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(elapsed).count() / 1000.0;
}

std::string format_seconds(double seconds)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << seconds << "s";
    return oss.str();
}

double read_env_double(const char* key, double fallback)
{
    const char* value = std::getenv(key);
    if (!value || !*value) {
        return fallback;
    }
    try {
        size_t idx = 0;
        const std::string value_string = trim_copy(value);
        const double parsed = std::stod(value_string, &idx);
        if (idx != value_string.size() || !std::isfinite(parsed) || parsed <= 0.0) {
            return fallback;
        }
        return parsed;
    } catch (const std::exception&) {
        return fallback;
    }
}


PerfThresholds thresholds_for_choice(LLMChoice choice)
{
    switch (choice) {
    case LLMChoice::Local_4b_Qwen35:
        return PerfThresholds{
            read_env_double("BENCH_CAT_LOCAL_QWEN35_4B_OPTIMAL_MAX", 2.0),
            read_env_double("BENCH_CAT_LOCAL_QWEN35_4B_ACCEPTABLE_MAX", 5.0)};
    case LLMChoice::Local_4b_Gemma:
        return PerfThresholds{
            read_env_double("BENCH_CAT_LOCAL_3B_OPTIMAL_MAX", 2.0),
            read_env_double("BENCH_CAT_LOCAL_3B_ACCEPTABLE_MAX", 5.0)};
    case LLMChoice::Local_3b_legacy:
        return PerfThresholds{
            read_env_double("BENCH_CAT_LOCAL_3B_LEGACY_OPTIMAL_MAX", 5.0),
            read_env_double("BENCH_CAT_LOCAL_3B_LEGACY_ACCEPTABLE_MAX", 10.0)};
    case LLMChoice::Local_7b:
        return PerfThresholds{
            read_env_double("BENCH_CAT_LOCAL_7B_OPTIMAL_MAX", 8.0),
            read_env_double("BENCH_CAT_LOCAL_7B_ACCEPTABLE_MAX", 12.0)};
    case LLMChoice::Local_7b_Gemma:
        return PerfThresholds{
            read_env_double("BENCH_CAT_LOCAL_7B_GEMMA_OPTIMAL_MAX", 8.0),
            read_env_double("BENCH_CAT_LOCAL_7B_GEMMA_ACCEPTABLE_MAX", 12.0)};
    default:
        return PerfThresholds{
            read_env_double("BENCH_CAT_DEFAULT_OPTIMAL_MAX", 5.0),
            read_env_double("BENCH_CAT_DEFAULT_ACCEPTABLE_MAX", 10.0)};
    }
}

PerfThresholds thresholds_for_document_choice(LLMChoice choice)
{
    switch (choice) {
    case LLMChoice::Local_4b_Qwen35:
        return PerfThresholds{
            read_env_double("BENCH_DOC_LOCAL_QWEN35_4B_OPTIMAL_MAX", 4.0),
            read_env_double("BENCH_DOC_LOCAL_QWEN35_4B_ACCEPTABLE_MAX", 7.0)};
    case LLMChoice::Local_4b_Gemma:
        return PerfThresholds{
            read_env_double("BENCH_DOC_LOCAL_3B_OPTIMAL_MAX", 4.0),
            read_env_double("BENCH_DOC_LOCAL_3B_ACCEPTABLE_MAX", 7.0)};
    case LLMChoice::Local_3b_legacy:
        return PerfThresholds{
            read_env_double("BENCH_DOC_LOCAL_3B_LEGACY_OPTIMAL_MAX", 8.0),
            read_env_double("BENCH_DOC_LOCAL_3B_LEGACY_ACCEPTABLE_MAX", 12.0)};
    case LLMChoice::Local_7b:
        return PerfThresholds{
            read_env_double("BENCH_DOC_LOCAL_7B_OPTIMAL_MAX", 12.0),
            read_env_double("BENCH_DOC_LOCAL_7B_ACCEPTABLE_MAX", 15.0)};
    case LLMChoice::Local_7b_Gemma:
        return PerfThresholds{
            read_env_double("BENCH_DOC_LOCAL_7B_GEMMA_OPTIMAL_MAX", 12.0),
            read_env_double("BENCH_DOC_LOCAL_7B_GEMMA_ACCEPTABLE_MAX", 15.0)};
    default:
        return PerfThresholds{
            read_env_double("BENCH_DOC_DEFAULT_OPTIMAL_MAX", 8.0),
            read_env_double("BENCH_DOC_DEFAULT_ACCEPTABLE_MAX", 12.0)};
    }
}

PerfThresholds image_thresholds()
{
    return PerfThresholds{
        read_env_double("BENCH_IMAGE_OPTIMAL_MAX", 15.0),
        read_env_double("BENCH_IMAGE_ACCEPTABLE_MAX", 25.0)};
}

PerfClass classify_perf(double seconds, const PerfThresholds& thresholds)
{
    if (seconds <= thresholds.optimal_max) {
        return PerfClass::Optimal;
    }
    if (seconds <= std::max(thresholds.optimal_max, thresholds.acceptable_max)) {
        return PerfClass::Acceptable;
    }
    return PerfClass::TooLong;
}

int perf_rank(PerfClass perf)
{
    switch (perf) {
    case PerfClass::Optimal:
        return 0;
    case PerfClass::Acceptable:
        return 1;
    case PerfClass::TooLong:
        return 2;
    }
    return 2;
}

PerfClass worst_perf(PerfClass a, PerfClass b)
{
    return perf_rank(a) >= perf_rank(b) ? a : b;
}

QColor blend_colors(const QColor& foreground,
                    const QColor& background,
                    double foreground_weight)
{
    const double clamped_weight = std::clamp(foreground_weight, 0.0, 1.0);
    const double background_weight = 1.0 - clamped_weight;
    return QColor(static_cast<int>(std::round(foreground.red() * clamped_weight +
                                              background.red() * background_weight)),
                  static_cast<int>(std::round(foreground.green() * clamped_weight +
                                              background.green() * background_weight)),
                  static_cast<int>(std::round(foreground.blue() * clamped_weight +
                                              background.blue() * background_weight)));
}

QString css_color(const QColor& color)
{
    return color.name(QColor::HexRgb);
}

bool is_windows_11_or_newer()
{
#if defined(Q_OS_WIN)
    const QOperatingSystemVersion version = QOperatingSystemVersion::current();
    if (version.type() != QOperatingSystemVersion::Windows) {
        return false;
    }
    return version.majorVersion() > 10 ||
           (version.majorVersion() == 10 && version.microVersion() >= 22000);
#else
    return false;
#endif
}

QString perf_color(PerfClass perf)
{
    switch (perf) {
    case PerfClass::Optimal:
        return QStringLiteral("#1f6feb");
    case PerfClass::Acceptable:
        return QStringLiteral("#f2c200");
    case PerfClass::TooLong:
        return QStringLiteral("#d73a49");
    }
    return QStringLiteral("#1f6feb");
}

QString highlight_figures(const QString& text)
{
    static const QRegularExpression figure_pattern(QStringLiteral(R"(\b\d+(?:\.\d+)?\s*(?:MiB|s)\b)"));
    const QString escaped = text.toHtmlEscaped();
    QString result;
    result.reserve(escaped.size() + 32);

    int last_pos = 0;
    auto match_it = figure_pattern.globalMatch(escaped);
    while (match_it.hasNext()) {
        const auto match = match_it.next();
        const int start = match.capturedStart();
        const int end = match.capturedEnd();
        if (start < last_pos) {
            continue;
        }
        result += escaped.mid(last_pos, start - last_pos);
        result += QStringLiteral("<span style=\"color:#1f6feb;\">%1</span>").arg(match.captured(0));
        last_pos = end;
    }
    result += escaped.mid(last_pos);
    return result;
}

QString bold_llm_label(const std::string& label)
{
    return QStringLiteral("<b>%1</b>").arg(QString::fromStdString(label).toHtmlEscaped());
}

QString colored_seconds(double seconds, PerfClass perf)
{
    return QStringLiteral("<span style=\"color:%1;\">%2</span>")
        .arg(perf_color(perf))
        .arg(QString::fromStdString(format_seconds(seconds)));
}

QString colored_seconds_list(const std::vector<double>& seconds,
                             const std::function<PerfClass(double)>& classifier)
{
    QStringList parts;
    parts.reserve(static_cast<int>(seconds.size()));
    for (double value : seconds) {
        const PerfClass perf = classifier(value);
        parts << colored_seconds(value, perf);
    }
    return parts.join(QStringLiteral(", "));
}

std::string format_mib(size_t bytes)
{
    constexpr double kToMiB = 1024.0 * 1024.0;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / kToMiB) << " MiB";
    return oss.str();
}

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string read_env_lower(const char* key)
{
    const char* value = std::getenv(key);
    if (!value || !*value) {
        return {};
    }
    return to_lower_copy(value);
}

std::optional<std::string> read_env_value(const char* key)
{
    const char* value = std::getenv(key);
    if (!value) {
        return std::nullopt;
    }
    return std::string(value);
}

void set_env_value(const char* key, const std::optional<std::string>& value)
{
#if defined(_WIN32)
    if (value.has_value()) {
        _putenv_s(key, value->c_str());
    } else {
        _putenv_s(key, "");
    }
#else
    if (value.has_value()) {
        setenv(key, value->c_str(), 1);
    } else {
        unsetenv(key);
    }
#endif
}

bool case_insensitive_contains(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) {
        return true;
    }
    const std::string hay = to_lower_copy(std::string(haystack));
    const std::string nee = to_lower_copy(std::string(needle));
    return hay.find(nee) != std::string::npos;
}

bool is_metal_backend_key(std::string_view value)
{
    const std::string lowered = to_lower_copy(std::string(value));
    return lowered == "metal" || lowered == "mtl";
}

bool backend_identifier_matches(std::string_view actual, std::string_view requested)
{
    if (requested.empty()) {
        return true;
    }
    if (case_insensitive_contains(actual, requested)) {
        return true;
    }
    return is_metal_backend_key(requested) &&
           (case_insensitive_contains(actual, "metal") ||
            case_insensitive_contains(actual, "mtl"));
}

ggml_backend_reg_t backend_reg_by_name_or_alias(std::string_view backend_name)
{
    if (backend_name.empty()) {
        return nullptr;
    }

    const std::string name(backend_name);
    if (ggml_backend_reg_t reg = ggml_backend_reg_by_name(name.c_str())) {
        return reg;
    }

    if (is_metal_backend_key(backend_name)) {
        if (ggml_backend_reg_t reg = ggml_backend_reg_by_name("MTL")) {
            return reg;
        }
        return ggml_backend_reg_by_name("Metal");
    }

    return nullptr;
}

std::string format_timestamp(std::chrono::system_clock::time_point point)
{
    std::time_t time_value = std::chrono::system_clock::to_time_t(point);
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &time_value);
#else
    localtime_r(&time_value, &local_time);
#endif
    std::ostringstream oss;
    oss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

struct LocalModel {
    std::string label;
    std::filesystem::path path;
    LLMChoice choice;
};

struct BackendTarget {
    std::string key;
    std::string label;
};

struct BenchmarkBackendInfo {
    bool cuda_available{false};
    bool vulkan_available{false};
    bool metal_available{false};
    std::string vulkan_device;
    std::string metal_device;
    std::optional<std::string> blas_label;
};

bool any_gpu_backend_available(const BenchmarkBackendInfo& info)
{
    return info.cuda_available || info.vulkan_available || info.metal_available;
}

struct EnvEntry {
    std::string key;
    std::optional<std::string> value;
};

class EnvSnapshot {
public:
    template<std::size_t N>
    static EnvSnapshot capture(const std::array<const char*, N>& keys)
    {
        EnvSnapshot snapshot;
        for (const char* key : keys) {
            snapshot.entries_.push_back(EnvEntry{key, read_env_value(key)});
        }
        return snapshot;
    }

    void restore() const
    {
        for (const auto& entry : entries_) {
            set_env_value(entry.key.c_str(), entry.value);
        }
    }

private:
    std::vector<EnvEntry> entries_;
};

class ScopedEnvRestore {
public:
    explicit ScopedEnvRestore(const EnvSnapshot& snapshot)
        : snapshot_(snapshot)
    {
    }

    ~ScopedEnvRestore()
    {
        snapshot_.restore();
    }

private:
    const EnvSnapshot& snapshot_;
};

BackendTarget resolve_auto_backend_target(const BenchmarkBackendInfo& info)
{
    if (info.cuda_available) {
        return {"cuda", QObject::tr("CUDA").toStdString()};
    }
    if (info.vulkan_available) {
        return {"vulkan", QObject::tr("Vulkan").toStdString()};
    }
    if (info.metal_available) {
        return {"metal", QObject::tr("Metal").toStdString()};
    }
    return {"auto", QObject::tr("Auto").toStdString()};
}

BackendTarget resolve_backend_target(std::string_view override_value,
                                     const BenchmarkBackendInfo& info)
{
    const std::string backend = to_lower_copy(std::string(override_value));
    if (backend == "cpu") {
        return {"cpu", QObject::tr("CPU").toStdString()};
    }
    if (is_metal_backend_key(backend)) {
        return {"metal", QObject::tr("Metal").toStdString()};
    }
    if (backend == "cuda") {
        return {"cuda", QObject::tr("CUDA").toStdString()};
    }
    if (backend == "vulkan") {
        return {"vulkan", QObject::tr("Vulkan").toStdString()};
    }
    if (backend.empty() || backend == "auto") {
        return resolve_auto_backend_target(info);
    }
    return {"auto",
            QObject::tr("%1 (auto)").arg(QString::fromStdString(std::string(override_value))).toStdString()};
}

std::vector<LocalModel> collect_local_models(const std::vector<CustomLLM>& custom_llms)
{
    std::vector<LocalModel> models;
    std::unordered_set<std::string> seen;
    for (const auto& entry : default_llm_entries()) {
        auto path = resolve_downloaded_builtin_llm_path(entry.choice);
        if (!path || !has_gguf_header(*path)) {
            continue;
        }
        std::error_code ec;
        const auto canonical_path = std::filesystem::weakly_canonical(*path, ec);
        if (ec) {
            continue;
        }
        const std::string path_key = canonical_path.string();
        if (!seen.insert(path_key).second) {
            continue;
        }
        models.push_back(LocalModel{default_llm_label(entry).toStdString(), *path, entry.choice});
    }
    for (const auto& custom : custom_llms) {
        if (!is_valid_custom_llm(custom)) {
            continue;
        }
        std::filesystem::path path;
        try {
            path = Utils::utf8_to_path(custom.path);
        } catch (const std::exception&) {
            continue;
        }
        if (!has_gguf_header(path)) {
            continue;
        }
        std::error_code ec;
        const auto canonical_path = std::filesystem::weakly_canonical(path, ec);
        if (ec || !seen.insert(canonical_path.string()).second) {
            continue;
        }
        models.push_back(LocalModel{QObject::tr("Custom: %1")
                                          .arg(QString::fromStdString(custom.name)).toStdString(),
                                      path, LLMChoice::Custom});
    }

    return models;
}

void load_ggml_backends_once();

std::optional<std::string> detect_blas_backend_label()
{
    load_ggml_backends_once();

    const size_t device_count = ggml_backend_dev_count();
    for (size_t i = 0; i < device_count; ++i) {
        auto* device = ggml_backend_dev_get(i);
        if (!device) {
            continue;
        }
        const auto type = ggml_backend_dev_type(device);
        if (type != GGML_BACKEND_DEVICE_TYPE_ACCEL &&
            type != GGML_BACKEND_DEVICE_TYPE_CPU) {
            continue;
        }

        const char* dev_name = ggml_backend_dev_name(device);
        const char* dev_desc = ggml_backend_dev_description(device);
        auto* reg = ggml_backend_dev_backend_reg(device);
        const char* reg_name = reg ? ggml_backend_reg_name(reg) : nullptr;

        auto matches = [&](std::string_view needle) {
            return case_insensitive_contains(dev_name ? dev_name : "", needle) ||
                   case_insensitive_contains(dev_desc ? dev_desc : "", needle) ||
                   case_insensitive_contains(reg_name ? reg_name : "", needle);
        };

        if (matches("openblas")) {
            return std::string("OpenBLAS");
        }
        if (matches("accelerate")) {
            return std::string("Accelerate");
        }
        if (matches("mkl")) {
            return std::string("MKL");
        }
        if (matches("blis")) {
            return std::string("BLIS");
        }
        if (matches("blas")) {
            return std::string("BLAS");
        }
    }

    return std::nullopt;
}

struct BackendMemorySnapshot {
    size_t free_bytes{0};
    size_t total_bytes{0};
    std::string name;
};

void load_ggml_backends_once()
{
    GgmlRuntimePaths::ensure_ggml_backends_loaded(Logger::get_logger("core_logger"));
}

std::optional<BackendMemorySnapshot> query_backend_memory(std::string_view backend_name)
{
    load_ggml_backends_once();

    const size_t device_count = ggml_backend_dev_count();
    BackendMemorySnapshot best{};
    bool found = false;

    for (size_t i = 0; i < device_count; ++i) {
        auto* device = ggml_backend_dev_get(i);
        if (!device) {
            continue;
        }
        if (ggml_backend_dev_type(device) != GGML_BACKEND_DEVICE_TYPE_GPU) {
            continue;
        }
        auto* reg = ggml_backend_dev_backend_reg(device);
        const char* reg_name = reg ? ggml_backend_reg_name(reg) : nullptr;
        if (!backend_identifier_matches(reg_name ? reg_name : "", backend_name)) {
            continue;
        }

        size_t free_bytes = 0;
        size_t total_bytes = 0;
        ggml_backend_dev_memory(device, &free_bytes, &total_bytes);
        if (free_bytes == 0 && total_bytes == 0) {
            continue;
        }

        if (!found || total_bytes > best.total_bytes) {
            best.free_bytes = free_bytes;
            best.total_bytes = (total_bytes != 0) ? total_bytes : free_bytes;
            const char* dev_name = ggml_backend_dev_name(device);
            best.name = dev_name ? dev_name : "";
            found = true;
        }
    }

    if (found) {
        return best;
    }
    return std::nullopt;
}

bool is_backend_available(std::string_view backend_name)
{
    load_ggml_backends_once();
    ggml_backend_reg_t reg = backend_reg_by_name_or_alias(backend_name);
    if (!reg) {
        return false;
    }
    return ggml_backend_reg_dev_count(reg) > 0;
}

QString build_cpu_backend_note(const QString& reason,
                               const std::optional<std::string>& blas_label)
{
    QString details = reason;
    if (blas_label.has_value()) {
        if (!details.isEmpty()) {
            details += QStringLiteral("; ");
        }
        details += QString::fromStdString(*blas_label);
    }
    if (details.isEmpty()) {
        return QObject::tr("CPU");
    }
    return QObject::tr("CPU (%1)").arg(details);
}

QString build_gpu_backend_note(const BackendTarget& target)
{
    return QObject::tr("GPU (target: %1)").arg(QString::fromStdString(target.label));
}

std::optional<QString> backend_unavailable_reason(const BackendTarget& target,
                                                  const BenchmarkBackendInfo& info)
{
    if (target.key == "cuda" && !info.cuda_available) {
        QString reason = QObject::tr("GPU via CUDA unavailable");
        if (!info.vulkan_available) {
            reason += QStringLiteral("; ") + QObject::tr("Vulkan unavailable");
        }
        return reason;
    }
    if (target.key == "vulkan" && !info.vulkan_available) {
        return QObject::tr("GPU via Vulkan unavailable");
    }
    if (target.key == "metal" && !info.metal_available) {
        return QObject::tr("GPU via Metal unavailable");
    }
    if (target.key == "auto" && !any_gpu_backend_available(info)) {
        return QObject::tr("GPU init failed");
    }
    return std::nullopt;
}

QString build_backend_note(const BackendTarget& target,
                           const BenchmarkBackendInfo& info,
                           bool gpu_fallback)
{
    if (target.key == "cpu") {
        return build_cpu_backend_note(QString(), info.blas_label);
    }

    if (gpu_fallback) {
        return build_cpu_backend_note(
            backend_unavailable_reason(target, info).value_or(QObject::tr("GPU init failed")),
            info.blas_label);
    }

    if (const auto reason = backend_unavailable_reason(target, info)) {
        return build_cpu_backend_note(*reason, info.blas_label);
    }

    return build_gpu_backend_note(target);
}

QString build_image_backend_note(std::string_view requested_backend,
                                 bool gpu_enabled,
                                 const BenchmarkBackendInfo& info)
{
    if (!gpu_enabled) {
        return build_cpu_backend_note(QObject::tr("GPU disabled by backend override"), std::nullopt);
    }

    const BackendTarget target = resolve_backend_target(requested_backend, info);
    if (target.key == "cpu") {
        return build_cpu_backend_note(QObject::tr("GPU disabled by backend override"), std::nullopt);
    }
    return build_backend_note(target, info, false);
}

std::string join_duration_list(const std::vector<double>& seconds)
{
    std::ostringstream oss;
    for (size_t i = 0; i < seconds.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << format_seconds(seconds[i]);
    }
    return oss.str();
}

double median_seconds(std::vector<double> seconds)
{
    if (seconds.empty()) {
        return 0.0;
    }
    std::sort(seconds.begin(), seconds.end());
    const size_t mid = seconds.size() / 2;
    if (seconds.size() % 2 == 1) {
        return seconds[mid];
    }
    return (seconds[mid - 1] + seconds[mid]) / 2.0;
}

struct VisualModel {
    std::string id;
    std::string label;
    VisualLlmRuntime::Backend backend;
};

std::vector<VisualModel> collect_visual_models(const std::vector<CustomLLM>& custom_llms)
{
    std::vector<VisualModel> models;
    const auto add_model = [&](std::string id, std::string label) {
        if (auto backend = VisualLlmRuntime::resolve_active_backend(id, custom_llms)) {
            models.push_back({std::move(id), std::move(label), std::move(*backend)});
        }
    };
    for (const auto& descriptor : visual_model_descriptors()) {
        add_model(descriptor.id, descriptor.display_name);
    }
    for (const auto& custom : custom_llms) {
        if (!is_visual_custom_llm(custom)) continue;
        add_model(custom_visual_model_id_for_llm(custom.id),
                  QObject::tr("Custom: %1").arg(QString::fromStdString(custom.name)).toStdString());
    }
    return models;
}

bool has_any_llm_available(const std::vector<CustomLLM>& custom_llms)
{
    return !collect_local_models(custom_llms).empty() || !collect_visual_models(custom_llms).empty();
}

std::filesystem::path create_temp_dir()
{
    auto ensure_writable_dir = [](const std::filesystem::path& dir) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec || !std::filesystem::exists(dir)) {
            return false;
        }

        const std::filesystem::path probe = dir / ".aifs-write-probe";
        std::ofstream out(probe, std::ios::out | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << "ok";
        out.close();
        std::filesystem::remove(probe, ec);
        return true;
    };

    std::vector<std::filesystem::path> roots;
    std::error_code ec;
    std::filesystem::path temp_root = std::filesystem::temp_directory_path(ec);
    if (!ec && !temp_root.empty()) {
        roots.push_back(temp_root);
    }
    if (const char* tmpdir = std::getenv("TMPDIR"); tmpdir && *tmpdir) {
        roots.emplace_back(tmpdir);
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        roots.emplace_back(std::filesystem::path(home) / ".cache");
    }
    ec.clear();
    std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (!ec && !cwd.empty()) {
        roots.push_back(cwd);
    }

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    for (const auto& root : roots) {
        std::filesystem::path base = root / "aifs-benchmark";
        if (!ensure_writable_dir(base)) {
            continue;
        }

        for (int attempt = 0; attempt < 16; ++attempt) {
            std::filesystem::path run_dir = base / ("run-" + std::to_string(stamp) +
                                                    "-" + std::to_string(attempt));
            std::error_code create_error;
            if (!std::filesystem::create_directory(run_dir, create_error)) {
                continue;
            }
            if (ensure_writable_dir(run_dir)) {
                return run_dir;
            }
            std::filesystem::remove_all(run_dir, create_error);
        }
    }

    return std::filesystem::path();
}

bool write_sample_document(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::ofstream out(path);
    if (!out) {
        return false;
    }
    out << "Project Phoenix Q1 Summary\n";
    out << "This report covers milestones, risks, and budget updates for Q1 2025.\n";
    out << "Key topics include cloud migration, incident response, and hiring plans.\n";
    out << "Overall progress is steady with a focus on cost optimization and delivery timelines.\n";
    return static_cast<bool>(out);
}

bool write_sample_image(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }

    QImage image(96, 96, QImage::Format_RGB32);
    image.fill(QColor(30, 120, 200));
    QPainter painter(&image);
    painter.fillRect(QRect(18, 18, 60, 60), QColor(240, 200, 60));
    painter.end();
    return image.save(QString::fromStdString(Utils::path_to_utf8(path)), "PNG");
}

std::optional<std::pair<std::string, std::string>> parse_category_pair(const std::string& response)
{
    std::istringstream stream(response);
    std::string line;
    std::string category;
    std::string subcategory;

    while (std::getline(stream, line)) {
        std::string trimmed = trim_copy(line);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed.rfind("Category", 0) == 0) {
            const auto pos = trimmed.find(':');
            if (pos != std::string::npos) {
                category = trim_copy(trimmed.substr(pos + 1));
            }
            continue;
        }

        if (trimmed.rfind("Subcategory", 0) == 0) {
            const auto pos = trimmed.find(':');
            if (pos != std::string::npos) {
                subcategory = trim_copy(trimmed.substr(pos + 1));
            }
            continue;
        }

        if (!trimmed.empty() && (trimmed.front() == '-' || trimmed.front() == '*')) {
            trimmed = trim_copy(trimmed.substr(1));
        }

        size_t pos = trimmed.find(" : ");
        size_t delim_len = 0;
        if (pos != std::string::npos) {
            delim_len = 3;
        } else {
            pos = trimmed.find(':');
            delim_len = (pos != std::string::npos) ? 1 : 0;
        }

        if (pos != std::string::npos && delim_len > 0) {
            const std::string raw_category = trim_copy(trimmed.substr(0, pos));
            const std::string raw_subcategory = trim_copy(trimmed.substr(pos + delim_len));
            if (!raw_category.empty() && !raw_subcategory.empty()) {
                category = Utils::sanitize_path_label(raw_category);
                subcategory = Utils::sanitize_path_label(raw_subcategory);
                break;
            }
        }
    }

    if (!category.empty() && !subcategory.empty()) {
        return std::make_pair(category, subcategory);
    }
    return std::nullopt;
}

struct StepResult {
    bool success{false};
    bool skipped{false};
    std::string backend_key;
    std::string backend_fallback_reason;
    std::string detail;
    std::chrono::steady_clock::duration duration{};
};

std::string image_backend_fallback_reason(const std::string& backend_key,
                                          const BenchmarkBackendInfo& backend_info)
{
#if defined(__APPLE__)
    const auto target = resolve_backend_target(backend_key, backend_info);
    if (const auto reason = backend_unavailable_reason(target, backend_info)) {
        return reason->toStdString();
    }
#else
    (void)backend_key;
#endif
    (void)backend_info;
    return {};
}

struct TextModelOutcome {
    bool skipped{true};
    bool categorization_ok{false};
    bool document_ok{false};
    std::optional<PerfClass> categorization_perf;
    std::optional<PerfClass> document_perf;
    struct ModelPerf {
        std::string label;
        LLMChoice choice{LLMChoice::Unset};
        PerfClass cat_perf{PerfClass::TooLong};
        PerfClass doc_perf{PerfClass::TooLong};
        double cat_median{0.0};
        double doc_median{0.0};
        bool cat_success{false};
        bool doc_success{false};
    };
    std::vector<ModelPerf> model_results;
    std::string detail;
};

StepResult run_categorization_test(ILLMClient& llm, const std::filesystem::path& temp_dir)
{
    StepResult result;
    const auto start = std::chrono::steady_clock::now();
    try {
        const std::string file_name = "Quarterly_Report_Q1_2025.pdf";
        const std::filesystem::path file_path = temp_dir / file_name;
        const std::string response = llm.categorize_file(file_name,
                                                         file_path.string(),
                                                         FileType::File,
                                                         std::string());
        const auto parsed = parse_category_pair(response);
        result.success = parsed.has_value();
        if (!result.success) {
            result.detail = "Unexpected response format.";
        }
    } catch (const std::exception& ex) {
        result.detail = ex.what();
    }
    result.duration = std::chrono::steady_clock::now() - start;
    return result;
}

StepResult run_document_test(ILLMClient& llm, const std::filesystem::path& temp_dir)
{
    StepResult result;
    if (temp_dir.empty()) {
        result.detail = "No writable temporary directory available.";
        return result;
    }

    const std::filesystem::path doc_path = temp_dir / "benchmark_document.txt";
    if (!write_sample_document(doc_path)) {
        result.detail = "Failed to create sample document.";
        return result;
    }

    const auto start = std::chrono::steady_clock::now();
    try {
        DocumentTextAnalyzer analyzer;
        const auto analysis = analyzer.analyze(doc_path, llm);
        result.success = !analysis.suggested_name.empty();
        if (!result.success) {
            result.detail = "Empty suggestion.";
        }
    } catch (const std::exception& ex) {
        result.detail = ex.what();
    }
    result.duration = std::chrono::steady_clock::now() - start;
    return result;
}

StepResult run_image_test(const std::filesystem::path& temp_dir,
                          std::string_view visual_backend_id,
                          const std::vector<CustomLLM>& custom_llms,
                          const BenchmarkBackendInfo& backend_info,
                          const std::string& required_backend)
{
    StepResult result;
#if defined(AI_FILE_SORTER_HAS_MTMD)
    if (temp_dir.empty()) {
        result.detail = "No writable temporary directory available.";
        return result;
    }

    std::string visual_error;
    auto visual_backend = VisualLlmRuntime::resolve_active_backend(visual_backend_id,
                                                                  custom_llms,
                                                                  &visual_error);
    if (!visual_backend) {
        result.skipped = true;
        result.detail = visual_error.empty() ? "Visual LLM files unavailable." : visual_error;
        return result;
    }

    const std::filesystem::path image_path = temp_dir / "benchmark_image.png";
    if (!write_sample_image(image_path)) {
        result.detail = "Failed to create sample image.";
        return result;
    }

    const auto start = std::chrono::steady_clock::now();
    try {
        ImageAnalyzerSettings analyzer_settings;
        result.backend_key = required_backend;
        analyzer_settings.required_backend = required_backend;
        result.backend_fallback_reason =
            image_backend_fallback_reason(result.backend_key, backend_info);
        analyzer_settings.use_gpu = result.backend_key != "cpu" &&
                                    result.backend_fallback_reason.empty();
        auto analyzer = ImageAnalyzerFactory::create(*visual_backend, analyzer_settings);
        const auto analysis = analyzer->analyze(image_path);
        result.success = !analysis.suggested_name.empty();
        if (required_backend != "cpu" && analysis.diagnostics.available
            && !analysis.diagnostics.text_gpu_enabled) {
            result.success = false;
            result.detail = "The visual model used CPU instead of the requested GPU backend.";
        } else if (!result.success) {
            result.detail = "Empty suggestion.";
        }
    } catch (const std::exception& ex) {
        result.detail = ex.what();
    }
    result.duration = std::chrono::steady_clock::now() - start;
#else
    (void)backend_info;
    (void)required_backend;
    result.skipped = true;
    result.detail = "Visual LLM support is not available in this build.";
#endif
    return result;
}

TextModelOutcome run_text_model_checks(const std::vector<LocalModel>& models,
                                       const std::filesystem::path& temp_dir,
                                       const EnvSnapshot& baseline_env,
                                       const BenchmarkBackendInfo& backend_info,
                                       const std::function<void(const QString&)>& post_line,
                                       const std::function<void(const QString&)>& post_line_html,
                                       const std::function<bool()>& should_stop,
                                       const std::string& required_backend)
{
    TextModelOutcome outcome;
    constexpr int kPerItemRuns = 3;
    if (models.empty()) {
        return outcome;
    }

    outcome.skipped = false;
    outcome.categorization_ok = true;
    outcome.document_ok = true;

    bool first_model = true;
    for (const auto& model : models) {
        if (should_stop && should_stop()) {
            break;
        }
        baseline_env.restore();
        ScopedEnvRestore env_restore(baseline_env);
        if (post_line) {
            if (!first_model) {
                post_line(QStringLiteral("----"));
            }
            if (post_line_html) {
                post_line_html(QObject::tr("Local model: %1").arg(bold_llm_label(model.label)));
            } else {
                post_line(QObject::tr("Local model: %1").arg(QString::fromStdString(model.label)));
            }
        }
        try {
            LocalLLMClient::Options options;
            options.force_cpu_backend = required_backend == "cpu";
            options.required_backend = required_backend;
            LocalLLMClient client(Utils::path_to_utf8(model.path),
                                  [](const std::string&) { return false; }, options);

            StepResult cat_warm;
            StepResult cat_init;
            std::vector<StepResult> cat_runs;
            StepResult doc_warm;
            StepResult doc_init;
            std::vector<StepResult> doc_runs;
            bool status_fallback = false;
            client.set_status_callback([&status_fallback](LocalLLMClient::Status status) {
                switch (status) {
                    case LocalLLMClient::Status::GpuLowMemoryFallbackToCpu:
                    case LocalLLMClient::Status::GpuFallbackToCpu:
                        status_fallback = true;
                        break;
                }
            });

            const std::string backend_before_cat = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
            const BackendTarget cat_target = resolve_backend_target(backend_before_cat, backend_info);
            if (post_line) {
                post_line(QObject::tr("    Measuring categorization (warm-up + %1 run(s))...")
                              .arg(kPerItemRuns + 1));
            }
            cat_warm = run_categorization_test(client, temp_dir);
            if (should_stop && should_stop()) {
                return outcome;
            }
            cat_init = run_categorization_test(client, temp_dir);
            if (should_stop && should_stop()) {
                return outcome;
            }
            cat_runs.reserve(static_cast<size_t>(kPerItemRuns));
            for (int i = 0; i < kPerItemRuns; ++i) {
                cat_runs.push_back(run_categorization_test(client, temp_dir));
                if (should_stop && should_stop()) {
                    return outcome;
                }
            }
            const std::string backend_after_cat = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
            const bool env_fallback_cat = (cat_target.key != "cpu" && backend_after_cat == "cpu");
            const bool cat_fallback = status_fallback || env_fallback_cat;
            bool cat_success = cat_warm.success && cat_init.success;
            for (const auto& run : cat_runs) {
                cat_success = cat_success && run.success;
            }

            status_fallback = false;
            const std::string backend_before_doc = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
            const BackendTarget doc_target = resolve_backend_target(backend_before_doc, backend_info);
            if (post_line) {
                post_line(QObject::tr("    Measuring document analysis (warm-up + %1 run(s))...")
                              .arg(kPerItemRuns + 1));
            }
            doc_warm = run_document_test(client, temp_dir);
            if (should_stop && should_stop()) {
                return outcome;
            }
            doc_init = run_document_test(client, temp_dir);
            if (should_stop && should_stop()) {
                return outcome;
            }
            doc_runs.reserve(static_cast<size_t>(kPerItemRuns));
            for (int i = 0; i < kPerItemRuns; ++i) {
                doc_runs.push_back(run_document_test(client, temp_dir));
                if (should_stop && should_stop()) {
                    return outcome;
                }
            }
            const std::string backend_after_doc = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
            const bool env_fallback_doc = (doc_target.key != "cpu" && backend_after_doc == "cpu");
            const bool doc_fallback = status_fallback || env_fallback_doc;
            bool doc_success = doc_warm.success && doc_init.success;
            for (const auto& run : doc_runs) {
                doc_success = doc_success && run.success;
            }

            if (required_backend != "cpu") {
                cat_success = cat_success && !cat_fallback;
                doc_success = doc_success && !doc_fallback;
            }
            outcome.categorization_ok = outcome.categorization_ok && cat_success;
            outcome.document_ok = outcome.document_ok && doc_success;

            std::vector<double> cat_per_seconds;
            cat_per_seconds.reserve(cat_runs.size());
            for (const auto& run : cat_runs) {
                cat_per_seconds.push_back(duration_seconds(run.duration));
            }
            const double cat_median = median_seconds(cat_per_seconds);
            const PerfClass cat_perf = cat_success
                ? classify_perf(cat_median, thresholds_for_choice(model.choice)) : PerfClass::TooLong;
            outcome.categorization_perf = outcome.categorization_perf.has_value()
                ? std::optional<PerfClass>(worst_perf(*outcome.categorization_perf, cat_perf))
                : std::optional<PerfClass>(cat_perf);

            std::vector<double> doc_per_seconds;
            doc_per_seconds.reserve(doc_runs.size());
            for (const auto& run : doc_runs) {
                doc_per_seconds.push_back(duration_seconds(run.duration));
            }
            const double doc_median = median_seconds(doc_per_seconds);
            const PerfClass doc_perf = doc_success
                ? classify_perf(doc_median, thresholds_for_document_choice(model.choice)) : PerfClass::TooLong;
            outcome.document_perf = outcome.document_perf.has_value()
                ? std::optional<PerfClass>(worst_perf(*outcome.document_perf, doc_perf))
                : std::optional<PerfClass>(doc_perf);

            if (!cat_success || !doc_success) {
                for (const auto* step : {&cat_warm, &cat_init, &doc_warm, &doc_init}) {
                    if (!step->success && !step->detail.empty()) { outcome.detail = step->detail; break; }
                }
                for (const auto& run : cat_runs) if (outcome.detail.empty() && !run.success) outcome.detail = run.detail;
                for (const auto& run : doc_runs) if (outcome.detail.empty() && !run.success) outcome.detail = run.detail;
                if (outcome.detail.empty()) outcome.detail = "The requested backend did not complete all checks.";
            }

            outcome.model_results.push_back(TextModelOutcome::ModelPerf{
                model.label,
                model.choice,
                cat_perf,
                doc_perf,
                cat_median,
                doc_median,
                cat_success,
                doc_success
            });

            if (post_line) {
                post_line(QObject::tr("Categorization: %1")
                              .arg(cat_success ? QObject::tr("done") : QObject::tr("failed")));
                if (post_line_html) {
                    post_line_html(QObject::tr("    Warm-up: %1")
                                      .arg(colored_seconds(duration_seconds(cat_warm.duration), cat_warm.success ? PerfClass::Optimal : PerfClass::TooLong)));
                    post_line_html(QObject::tr("    Init: %1")
                                      .arg(colored_seconds(duration_seconds(cat_init.duration), cat_init.success ? PerfClass::Optimal : PerfClass::TooLong)));
                    post_line_html(QObject::tr("    Per-item (median of %1): %2")
                                      .arg(cat_per_seconds.size())
                                      .arg(colored_seconds(cat_median, cat_perf)));
                    post_line_html(QObject::tr("    Per-item runs: %1")
                                      .arg(colored_seconds_list(
                                          cat_per_seconds,
                                          [model, cat_success](double value) {
                                              return cat_success
                                                  ? classify_perf(value, thresholds_for_choice(model.choice))
                                                  : PerfClass::TooLong;
                                          })));
                } else {
                    post_line(QObject::tr("    Warm-up: %1")
                                  .arg(QString::fromStdString(format_duration(cat_warm.duration))));
                    post_line(QObject::tr("    Init: %1")
                                  .arg(QString::fromStdString(format_duration(cat_init.duration))));
                    post_line(QObject::tr("    Per-item (median of %1): %2")
                                  .arg(cat_per_seconds.size())
                                  .arg(QString::fromStdString(format_seconds(cat_median))));
                    post_line(QObject::tr("    Per-item runs: %1")
                                  .arg(QString::fromStdString(join_duration_list(cat_per_seconds))));
                }
                if (!cat_success) {
                    std::string detail = cat_warm.detail;
                    if (detail.empty()) {
                        detail = cat_init.detail;
                    }
                    if (detail.empty()) {
                        for (const auto& run : cat_runs) {
                            if (!run.detail.empty()) {
                                detail = run.detail;
                                break;
                            }
                        }
                    }
                    if (!detail.empty()) {
                        post_line(QObject::tr("Details: %1").arg(QString::fromStdString(detail)));
                    }
                }
                post_line(QObject::tr("Backend used: %1")
                              .arg(build_backend_note(cat_target, backend_info, cat_fallback)));
                post_line(QObject::tr("Document analysis: %1")
                              .arg(doc_success ? QObject::tr("done") : QObject::tr("failed")));
                if (post_line_html) {
                    post_line_html(QObject::tr("    Warm-up: %1")
                                      .arg(colored_seconds(duration_seconds(doc_warm.duration), doc_warm.success ? PerfClass::Optimal : PerfClass::TooLong)));
                    post_line_html(QObject::tr("    Init: %1")
                                      .arg(colored_seconds(duration_seconds(doc_init.duration), doc_init.success ? PerfClass::Optimal : PerfClass::TooLong)));
                    post_line_html(QObject::tr("    Per-item (median of %1): %2")
                                      .arg(doc_per_seconds.size())
                                      .arg(colored_seconds(doc_median, doc_perf)));
                    post_line_html(QObject::tr("    Per-item runs: %1")
                                      .arg(colored_seconds_list(
                                          doc_per_seconds,
                                          [model, doc_success](double value) {
                                              return doc_success
                                                  ? classify_perf(value, thresholds_for_document_choice(model.choice))
                                                  : PerfClass::TooLong;
                                          })));
                } else {
                    post_line(QObject::tr("    Warm-up: %1")
                                  .arg(QString::fromStdString(format_duration(doc_warm.duration))));
                    post_line(QObject::tr("    Init: %1")
                                  .arg(QString::fromStdString(format_duration(doc_init.duration))));
                    post_line(QObject::tr("    Per-item (median of %1): %2")
                                  .arg(doc_per_seconds.size())
                                  .arg(QString::fromStdString(format_seconds(doc_median))));
                    post_line(QObject::tr("    Per-item runs: %1")
                                  .arg(QString::fromStdString(join_duration_list(doc_per_seconds))));
                }
                if (!doc_success) {
                    std::string detail = doc_warm.detail;
                    if (detail.empty()) {
                        detail = doc_init.detail;
                    }
                    if (detail.empty()) {
                        for (const auto& run : doc_runs) {
                            if (!run.detail.empty()) {
                                detail = run.detail;
                                break;
                            }
                        }
                    }
                    if (!detail.empty()) {
                        post_line(QObject::tr("Details: %1").arg(QString::fromStdString(detail)));
                    }
                }
                post_line(QObject::tr("Backend used: %1")
                              .arg(build_backend_note(doc_target, backend_info, doc_fallback)));
            }
        } catch (const std::exception& ex) {
            outcome.categorization_ok = false;
            outcome.document_ok = false;
            outcome.detail = ex.what();
            if (post_line) {
                post_line(QObject::tr("Model failed to load: %1")
                              .arg(QString::fromStdString(ex.what())));
            }
        }
        first_model = false;
    }

    return outcome;
}

QString perf_label_qt(PerfClass perf)
{
    switch (perf) {
    case PerfClass::Optimal:
        return QObject::tr("optimal");
    case PerfClass::Acceptable:
        return QObject::tr("acceptable");
    case PerfClass::TooLong:
        return QObject::tr("a bit long");
    }
    return QObject::tr("a bit long");
}

QString colored_perf_label(PerfClass perf)
{
    return QStringLiteral("<span style=\"color:%1; font-weight:600;\">%2</span>")
        .arg(perf_color(perf))
        .arg(perf_label_qt(perf));
}

QString build_recommended_list(const QStringList& labels)
{
    QStringList items;
    items.reserve(labels.size());
    for (const auto& label : labels) {
        const QString trimmed = label.trimmed();
        if (!trimmed.isEmpty()) {
            items << trimmed;
        }
    }
    if (items.empty()) {
        items << QObject::tr("n/a");
    }
    QString html;
    for (int i = 0; i < items.size(); ++i) {
        if (i > 0) {
            html += QStringLiteral("<br>");
        }
        html += QStringLiteral("- %1").arg(items[i].toHtmlEscaped());
    }
    return html;
}

QStringList build_result_lines(const TextModelOutcome& text_models,
                               const StepResult& image,
                               SortingMode sorting_mode)
{
    QStringList lines;
    lines << QStringLiteral("<span style=\"color:#1f6feb; font-weight:700;\">%1</span>")
                 .arg(QObject::tr("Result"));

    if (text_models.skipped) {
        lines << QObject::tr("Categorization speed: unavailable");
        lines << QObject::tr("Document analysis speed: unavailable");
    } else {
        std::optional<PerfClass> cat_perf;
        std::optional<PerfClass> doc_perf;
        for (const auto& entry : text_models.model_results) {
            if (entry.cat_success && (!cat_perf || perf_rank(entry.cat_perf) < perf_rank(*cat_perf))) {
                cat_perf = entry.cat_perf;
            }
            if (entry.doc_success && (!doc_perf || perf_rank(entry.doc_perf) < perf_rank(*doc_perf))) {
                doc_perf = entry.doc_perf;
            }
        }
        lines << QObject::tr("Categorization speed: %1")
                     .arg(cat_perf ? colored_perf_label(*cat_perf) : QObject::tr("failed"));
        lines << QObject::tr("Document analysis speed: %1")
                     .arg(doc_perf ? colored_perf_label(*doc_perf) : QObject::tr("failed"));
    }

    if (image.skipped) {
        lines << QObject::tr("Image analysis speed: unavailable");
    } else if (image.success) {
        const double seconds = duration_seconds(image.duration);
        const PerfClass image_perf = classify_perf(seconds, image_thresholds());
        lines << QObject::tr("Image analysis speed: %1").arg(colored_perf_label(image_perf));
    } else {
        lines << QObject::tr("Image analysis speed: %1").arg(QObject::tr("failed"));
    }

    QStringList recommended_labels;
    if (!text_models.model_results.empty()) {
        std::vector<const TextModelOutcome::ModelPerf*> optimal;
        std::vector<const TextModelOutcome::ModelPerf*> acceptable;
        std::vector<const TextModelOutcome::ModelPerf*> quite_long;
        for (const auto& entry : text_models.model_results) {
            if (!entry.cat_success || !entry.doc_success
                || (sorting_mode == SortingMode::ExistingFolderTree
                    && !is_recommended_for_existing_folder_sorting(entry.choice))) {
                continue;
            }
            if (entry.cat_perf == PerfClass::Optimal && entry.doc_perf == PerfClass::Optimal) {
                optimal.push_back(&entry);
            } else if (entry.cat_perf != PerfClass::TooLong && entry.doc_perf != PerfClass::TooLong) {
                acceptable.push_back(&entry);
            } else {
                quite_long.push_back(&entry);
            }
        }

        if (optimal.size() == 1) {
            recommended_labels << QString::fromStdString(optimal.front()->label);
        } else if (optimal.size() > 1) {
            for (const auto* entry : optimal) {
                recommended_labels << QString::fromStdString(entry->label);
            }
        } else if (acceptable.size() == 1) {
            recommended_labels << QString::fromStdString(acceptable.front()->label);
        } else if (acceptable.size() > 1) {
            for (const auto* entry : acceptable) {
                recommended_labels << QString::fromStdString(entry->label);
            }
        } else if (!quite_long.empty()) {
            double best_score = std::numeric_limits<double>::max();
            for (const auto* entry : quite_long) {
                const double score = entry->cat_median + entry->doc_median;
                best_score = std::min(best_score, score);
            }
            for (const auto* entry : quite_long) {
                const double score = entry->cat_median + entry->doc_median;
                if (std::abs(score - best_score) < 0.0001) {
                    recommended_labels << QString::fromStdString(entry->label);
                }
            }
        }
    }

    lines << QString();
    const QString recommended_header = QObject::tr("Recommended Local LLM choice: %1").arg(QString());
    lines << QStringLiteral("<span style=\"color:#1b9e3c; font-weight:700;\">%1</span>")
                 .arg(recommended_header.toHtmlEscaped());
    lines << build_recommended_list(recommended_labels);
    if (sorting_mode == SortingMode::ExistingFolderTree) {
        lines << QObject::tr("For existing folder structures, we recommend Qwen3.5 4B. "
                             "Gemma 1.1 7B and API models are alternatives. "
                             "These speed tests do not measure folder-routing accuracy.");
    }
    lines << QString();
    lines << QStringLiteral("<span style=\"color:#1f6feb;\">%1</span>")
                 .arg(QObject::tr("You can toggle LLMs in Settings -> Select LLM").toHtmlEscaped());
    return lines;
}

std::string file_identity(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (ec) return {};
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) return {};
    const auto modified = std::filesystem::last_write_time(path, ec);
    if (ec) return {};
    return Utils::path_to_utf8(canonical) + "|" + std::to_string(size) + "|"
        + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            modified.time_since_epoch()).count());
}

SuitabilityBenchmark::Results collect_candidates(const std::vector<CustomLLM>& customs)
{
    SuitabilityBenchmark::Results candidates;
    for (const auto& model : collect_local_models(customs)) {
        SuitabilityBenchmark::Result result;
        const auto identity = file_identity(model.path);
        if (identity.empty()) continue;
        result.model_key = "text:" + identity;
        result.model_path = Utils::path_to_utf8(model.path);
        result.label = model.label;
        result.choice = model.choice;
        candidates.push_back(std::move(result));
    }
    for (const auto& visual : collect_visual_models(customs)) {
        const auto model = visual.backend.path_for(VisualModelArtifactKind::Model);
        const auto projector = visual.backend.path_for(VisualModelArtifactKind::Mmproj);
        if (model && projector) {
            const auto model_identity = file_identity(*model);
            const auto projector_identity = file_identity(*projector);
            if (model_identity.empty() || projector_identity.empty()) continue;
            SuitabilityBenchmark::Result result;
            result.model_key = "visual:" + visual.id + "|" + model_identity + "|" + projector_identity;
            result.model_path = Utils::path_to_utf8(*model);
            result.visual_id = visual.id;
            result.label = QObject::tr("Image analysis: %1")
                .arg(QString::fromStdString(visual.label)).toStdString();
            result.visual = true;
            candidates.push_back(std::move(result));
        }
    }
    return candidates;
}

QString backend_label(const std::string& key)
{
    if (key == "cpu") return QObject::tr("CPU");
    if (key == "cuda") return QObject::tr("CUDA");
    if (key == "vulkan") return QObject::tr("Vulkan");
    if (key == "metal") return QObject::tr("Metal");
    return QString::fromStdString(key);
}

bool backend_available(const std::string& key, const BenchmarkBackendInfo& info)
{
    return key == "cpu" || (key == "cuda" && info.cuda_available)
        || (key == "vulkan" && info.vulkan_available) || (key == "metal" && info.metal_available);
}

std::vector<std::string> backend_plan(const std::string& mode, const BenchmarkBackendInfo& info,
                                     const std::string& original_override)
{
    if (mode == "all") {
        std::vector<std::string> keys;
        for (const auto* key : {"cuda", "vulkan", "metal", "cpu"}) {
            if (backend_available(key, info)) keys.emplace_back(key);
        }
        return keys;
    }
    if (mode != "auto") return {mode};
    const auto target = resolve_backend_target(original_override, info);
    return {target.key == "auto" ? "cpu" : target.key};
}

QStringList build_pair_summary(const SuitabilityBenchmark::Results& results, SortingMode mode,
                               const BenchmarkBackendInfo* available = nullptr)
{
    TextModelOutcome text;
    StepResult image;
    image.skipped = true;
    for (const auto& result : results) {
        if (available && !backend_available(result.backend, *available)) continue;
        if (result.visual) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(result.image_seconds));
            if (image.skipped || (result.image_success && (!image.success || elapsed < image.duration))) {
                image.skipped = false;
                image.success = result.image_success;
                image.duration = elapsed;
            }
        } else {
            text.skipped = false;
            text.model_results.push_back({result.label + " [" + backend_label(result.backend).toStdString() + "]",
                result.choice,
                classify_perf(result.categorization_seconds, thresholds_for_choice(result.choice)),
                classify_perf(result.document_seconds, thresholds_for_document_choice(result.choice)),
                result.categorization_seconds, result.document_seconds,
                result.categorization_success, result.document_success});
        }
    }
    return build_result_lines(text, image, mode);
}

} // namespace

#if defined(AI_FILE_SORTER_TEST_BUILD)
QStringList SuitabilityBenchmarkDialogTestAccess::result_lines_for_testing(
    const std::vector<MeasuredModel>& models, SortingMode mode)
{
    TextModelOutcome outcome;
    outcome.skipped = models.empty();
    for (const auto& model : models) {
        outcome.model_results.push_back({model.label, model.choice,
            classify_perf(model.categorization_seconds, thresholds_for_choice(model.choice)),
            classify_perf(model.document_seconds, thresholds_for_document_choice(model.choice)),
            model.categorization_seconds, model.document_seconds,
            model.categorization_success, model.document_success});
    }
    StepResult image;
    image.skipped = true;
    return build_result_lines(outcome, image, mode);
}

std::vector<std::string> SuitabilityBenchmarkDialogTestAccess::local_model_paths_for_testing(
    const std::vector<CustomLLM>& custom_llms)
{
    std::vector<std::string> paths;
    for (const auto& model : collect_local_models(custom_llms)) {
        paths.push_back(Utils::path_to_utf8(model.path));
    }
    return paths;
}

std::pair<double, double> SuitabilityBenchmarkDialogTestAccess::thresholds_for_testing(
    LLMChoice choice, bool document)
{
    const auto thresholds = document ? thresholds_for_document_choice(choice) : thresholds_for_choice(choice);
    return {thresholds.optimal_max, thresholds.acceptable_max};
}

void SuitabilityBenchmarkDialogTestAccess::DialogAccess::start(SuitabilityBenchmarkDialog& dialog)
{
    dialog.start_benchmark();
}

void SuitabilityBenchmarkDialogTestAccess::DialogAccess::set_running(
    SuitabilityBenchmarkDialog& dialog, bool running)
{
    dialog.set_running_state(running);
}

void SuitabilityBenchmarkDialogTestAccess::DialogAccess::finish(
    SuitabilityBenchmarkDialog& dialog, bool completed)
{
    dialog.finish_benchmark(completed);
}

void SuitabilityBenchmarkDialogTestAccess::DialogAccess::seed_results(
    SuitabilityBenchmarkDialog& dialog, SuitabilityBenchmark::Results results)
{
    dialog.results_ = std::move(results);
    dialog.refresh_results_table();
}

SuitabilityBenchmark::Results SuitabilityBenchmarkDialogTestAccess::DialogAccess::results(
    const SuitabilityBenchmarkDialog& dialog) { return dialog.results_; }

void SuitabilityBenchmarkDialogTestAccess::DialogAccess::set_runner(
    SuitabilityBenchmarkDialog& dialog,
    std::function<SuitabilityBenchmark::Result(const SuitabilityBenchmark::Result&)> runner,
    std::vector<std::string> available_backends)
{
    dialog.runner_override_ = std::move(runner);
    dialog.available_backends_override_ = std::move(available_backends);
}

QStringList SuitabilityBenchmarkDialogTestAccess::DialogAccess::summary(
    const SuitabilityBenchmarkDialog& dialog)
{
    return build_pair_summary(dialog.results_, dialog.settings_.get_sorting_mode());
}

bool SuitabilityBenchmarkDialogTestAccess::DialogAccess::running(
    const SuitabilityBenchmarkDialog& dialog) { return dialog.running_; }

bool SuitabilityBenchmarkDialogTestAccess::backend_alias_matches(
    std::string_view actual, std::string_view requested)
{
    return backend_identifier_matches(actual, requested);
}

QString SuitabilityBenchmarkDialogTestAccess::backend_note_for_testing(
    std::string_view requested_backend,
    bool cuda_available,
    bool vulkan_available,
    bool metal_available,
    bool gpu_fallback)
{
    BenchmarkBackendInfo info;
    info.cuda_available = cuda_available;
    info.vulkan_available = vulkan_available;
    info.metal_available = metal_available;
    return build_backend_note(resolve_backend_target(requested_backend, info), info, gpu_fallback);
}

QString SuitabilityBenchmarkDialogTestAccess::image_backend_note_for_testing(
    std::string_view requested_backend,
    bool gpu_enabled,
    bool cuda_available,
    bool vulkan_available,
    bool metal_available)
{
    BenchmarkBackendInfo info;
    info.cuda_available = cuda_available;
    info.vulkan_available = vulkan_available;
    info.metal_available = metal_available;
    return build_image_backend_note(requested_backend, gpu_enabled, info);
}
#endif

SuitabilityBenchmarkDialog::SuitabilityBenchmarkDialog(Settings& settings,
                                                       QWidget* parent,
                                                       std::filesystem::path runtime_directory)
    : QDialog(parent)
    , settings_(settings)
    , runtime_lock_(runtime_directory.empty()
                        ? Utils::utf8_to_path(settings.get_config_dir()) / "runtime"
                        : std::move(runtime_directory))
{
    setWindowModality(Qt::WindowModal);
    resize(820, 560);
    setup_ui();
    retranslate_ui();
    load_previous_results();
    refresh_results_table();
}

SuitabilityBenchmarkDialog::~SuitabilityBenchmarkDialog()
{
    stop_requested_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SuitabilityBenchmarkDialog::setup_ui()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    intro_label_ = new QLabel(this);
    intro_label_->setTextFormat(Qt::RichText);
    intro_label_->setWordWrap(true);
    layout->addWidget(intro_label_);

    auto* backend_row = new QHBoxLayout();
    auto* backend_caption = new QLabel(QObject::tr("Backend"), this);
    backend_caption->setObjectName(QStringLiteral("benchmarkBackendLabel"));
    backend_combo_ = new QComboBox(this);
    backend_combo_->setObjectName(QStringLiteral("benchmarkBackendCombo"));
    backend_combo_->addItem(QObject::tr("Automatic (try CPU if GPU fails)"), QStringLiteral("auto"));
    backend_combo_->addItem(QObject::tr("All available backends"), QStringLiteral("all"));
    for (const auto* backend : {"cpu", "cuda", "vulkan", "metal"}) {
        backend_combo_->addItem(backend_label(backend), QString::fromUtf8(backend));
    }
    backend_caption->setBuddy(backend_combo_);
    backend_row->addWidget(backend_caption);
    backend_row->addWidget(backend_combo_, 1);
    layout->addLayout(backend_row);

    results_table_ = new QTableWidget(0, 5, this);
    results_table_->setObjectName(QStringLiteral("benchmarkResultsTable"));
    results_table_->setHorizontalHeaderLabels({QObject::tr("Model"), QObject::tr("Backend"),
        QObject::tr("Categorization"), QObject::tr("Documents"), QObject::tr("Images")});
    results_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_table_->verticalHeader()->hide();
    results_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column) {
        results_table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    results_table_->setMinimumHeight(100);
    results_table_->setMaximumHeight(210);
    layout->addWidget(results_table_);
    auto* retry_row = new QHBoxLayout();
    retry_selected_button_ = new QPushButton(QObject::tr("Retry selected"), this);
    retry_selected_button_->setObjectName(QStringLiteral("benchmarkRetrySelected"));
    retry_failed_button_ = new QPushButton(QObject::tr("Retry failed"), this);
    retry_failed_button_->setObjectName(QStringLiteral("benchmarkRetryFailed"));
    try_backend_button_ = new QPushButton(QObject::tr("Try chosen backend"), this);
    try_backend_button_->setObjectName(QStringLiteral("benchmarkTryBackend"));
    try_backend_button_->setToolTip(QObject::tr("Test the selected model using the backend chosen above."));
    retry_row->addWidget(retry_selected_button_);
    retry_row->addWidget(retry_failed_button_);
    retry_row->addWidget(try_backend_button_);
    retry_row->addStretch(1);
    layout->addLayout(retry_row);
    connect(results_table_, &QTableWidget::itemSelectionChanged, this,
            &SuitabilityBenchmarkDialog::update_retry_controls);
    connect(retry_selected_button_, &QPushButton::clicked, this, [this]() { retry_selected(false); });
    connect(try_backend_button_, &QPushButton::clicked, this, [this]() { retry_selected(true); });
    connect(retry_failed_button_, &QPushButton::clicked, this, &SuitabilityBenchmarkDialog::retry_failed);
    update_retry_controls();

    auto* output_panel = new QFrame(this);
    output_panel->setObjectName(QStringLiteral("benchmarkActivityCard"));
    auto* output_layout = new QVBoxLayout(output_panel);
    output_layout->setContentsMargins(10, 10, 10, 10);
    output_layout->setSpacing(0);

    output_view_ = new QTextEdit(output_panel);
    output_view_->setObjectName(QStringLiteral("benchmarkActivityView"));
    output_view_->setReadOnly(true);
    output_view_->setAcceptRichText(true);
    output_view_->setLineWrapMode(QTextEdit::WidgetWidth);
    output_layout->addWidget(output_view_);
    layout->addWidget(output_panel, 1);

    auto* progress_row = new QWidget(this);
    progress_row->setObjectName(QStringLiteral("benchmarkProgressRow"));
    auto* progress_layout = new QHBoxLayout(progress_row);
    progress_layout->setContentsMargins(0, 2, 0, 0);
    progress_layout->setSpacing(0);
    progress_layout->addStretch(1);

    progress_bar_ = new QProgressBar(progress_row);
    progress_bar_->setObjectName(QStringLiteral("benchmarkProgressBar"));
    progress_bar_->setVisible(false);
    progress_bar_->setRange(0, 0);
    progress_bar_->setFixedWidth(320);
    progress_layout->addWidget(progress_bar_, 0, Qt::AlignCenter);
    progress_layout->addStretch(1);
    layout->addWidget(progress_row);

    auto* button_layout = new QHBoxLayout();
    suppress_checkbox_ = new QCheckBox(this);
    suppress_checkbox_->setChecked(settings_.get_suitability_benchmark_suppressed());
    button_layout->addWidget(suppress_checkbox_);
    button_layout->addStretch(1);

    stop_button_ = new QPushButton(this);
    stop_button_->setEnabled(false);
    button_layout->addWidget(stop_button_);

    run_button_ = new QPushButton(this);
    button_layout->addWidget(run_button_);

    close_button_ = new QPushButton(this);
    button_layout->addWidget(close_button_);

    layout->addLayout(button_layout);

    connect(run_button_, &QPushButton::clicked, this, &SuitabilityBenchmarkDialog::start_benchmark);
    connect(stop_button_, &QPushButton::clicked, this, &SuitabilityBenchmarkDialog::request_stop);
    connect(close_button_, &QPushButton::clicked, this, &QDialog::accept);
    connect(suppress_checkbox_, &QCheckBox::toggled, this, [this](bool checked) {
        settings_.set_suitability_benchmark_suppressed(checked);
        settings_.save();
    });

    apply_platform_styling();
}

void SuitabilityBenchmarkDialog::apply_platform_styling()
{
    if (!output_view_ || !progress_bar_) {
        return;
    }

    output_view_->document()->setDocumentMargin(4.0);
    progress_bar_->setTextVisible(false);

    if (!is_windows_11_or_newer()) {
        return;
    }

    output_view_->setFrameShape(QFrame::NoFrame);

    const QPalette palette = this->palette();
    const QColor window_color = palette.color(QPalette::Window);
    const QColor base_color = palette.color(QPalette::Base);
    const QColor text_color = palette.color(QPalette::WindowText);
    const QColor border_color = blend_colors(text_color, window_color, 0.14);
    const QColor panel_color = blend_colors(base_color, window_color, 0.82);
    const QColor progress_track_color = blend_colors(text_color, base_color, 0.08);
    const QColor progress_border_color = blend_colors(text_color, window_color, 0.18);
    const QColor accent_color = palette.color(QPalette::Highlight);

    setStyleSheet(QStringLiteral(
        "QFrame#benchmarkActivityCard {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
        "QTextEdit#benchmarkActivityView {"
        "  background-color: %1;"
        "  border: none;"
        "  padding: 0px;"
        "}"
        "QProgressBar#benchmarkProgressBar {"
        "  background-color: %3;"
        "  border: 1px solid %4;"
        "  border-radius: 5px;"
        "  min-height: 10px;"
        "  max-height: 10px;"
        "}"
        "QProgressBar#benchmarkProgressBar::chunk {"
        "  background-color: %5;"
        "  border-radius: 5px;"
        "}"
    ).arg(css_color(panel_color),
          css_color(border_color),
          css_color(progress_track_color),
          css_color(progress_border_color),
          css_color(accent_color)));
}

void SuitabilityBenchmarkDialog::retranslate_ui()
{
    setWindowTitle(QObject::tr("Compatibility Benchmark"));
    if (auto* caption = findChild<QLabel*>(QStringLiteral("benchmarkBackendLabel"))) {
        caption->setText(QObject::tr("Backend"));
    }
    if (backend_combo_) {
        backend_combo_->setItemText(0, QObject::tr("Automatic (try CPU if GPU fails)"));
        backend_combo_->setItemText(1, QObject::tr("All available backends"));
        for (int i = 2; i < backend_combo_->count(); ++i) {
            backend_combo_->setItemText(i, backend_label(backend_combo_->itemData(i).toString().toStdString()));
        }
    }
    if (results_table_) {
        results_table_->setHorizontalHeaderLabels({QObject::tr("Model"), QObject::tr("Backend"),
            QObject::tr("Categorization"), QObject::tr("Documents"), QObject::tr("Images")});
        refresh_results_table();
    }
    if (retry_selected_button_) retry_selected_button_->setText(QObject::tr("Retry selected"));
    if (retry_failed_button_) retry_failed_button_->setText(QObject::tr("Retry failed"));
    if (try_backend_button_) try_backend_button_->setText(QObject::tr("Try chosen backend"));
    if (intro_label_) {
        const QString intro_main = QObject::tr("Run a quick performance check to estimate how image analysis, document analysis, and file categorization will perform on your system.");
        const QString local_scope = QObject::tr("Tests all downloaded local text and visual models, including custom GGUF models. API services and folder-routing accuracy are not benchmarked.");
        const QString intro_warning = QObject::tr("It is recommended to quit any CPU- and GPU-intensive applications before running this test.");
        intro_label_->setText(QStringLiteral("%1<br><br>%2<br><br><span style=\"color:#d73a49; font-weight:600;\">%3</span>")
                                  .arg(intro_main.toHtmlEscaped(), local_scope.toHtmlEscaped(), intro_warning.toHtmlEscaped()));
    }
    if (run_button_) {
        run_button_->setText(QObject::tr("Run benchmark"));
    }
    if (suppress_checkbox_) {
        suppress_checkbox_->setText(QObject::tr("Do not auto-show this dialog again"));
    }
    if (stop_button_) {
        stop_button_->setText(QObject::tr("Stop Benchmark"));
    }
    if (close_button_) {
        close_button_->setText(QObject::tr("Close"));
    }
    if (showing_previous_results_) {
        render_previous_results();
    }
}

void SuitabilityBenchmarkDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange) {
        retranslate_ui();
    }
}

void SuitabilityBenchmarkDialog::load_previous_results()
{
    last_run_stamp_ = QString::fromStdString(settings_.get_benchmark_last_run());
    last_report_ = QString::fromStdString(settings_.get_benchmark_last_report());
    results_ = SuitabilityBenchmark::deserialize_results(settings_.get_benchmark_results_json());
    render_previous_results();
}

void SuitabilityBenchmarkDialog::render_previous_results()
{
    if (!output_view_) {
        return;
    }

    const bool was_recording = recording_;
    recording_ = false;
    output_view_->clear();
    showing_previous_results_ = true;

    if (last_report_.isEmpty()) {
        append_line(QObject::tr("No previous results yet."), false);
        recording_ = was_recording;
        return;
    }

    if (!last_run_stamp_.isEmpty()) {
        append_line(QObject::tr("Last run: %1").arg(last_run_stamp_), false);
    }
    append_line(QObject::tr("Previous results:"), false);

    const QStringList lines = last_report_.split('\n');
    for (const QString& line : lines) {
        append_line(line, line.contains('<'));
    }

    if (auto* scroll = output_view_->verticalScrollBar()) {
        scroll->setValue(scroll->maximum());
    }
    recording_ = was_recording;
}

void SuitabilityBenchmarkDialog::closeEvent(QCloseEvent* event)
{
    if (running_) {
        if (event) {
            event->ignore();
        }
        return;
    }
    QDialog::closeEvent(event);
}

bool SuitabilityBenchmarkDialog::models_available(const Settings& settings)
{
    return has_any_llm_available(settings.get_custom_llms());
}

void SuitabilityBenchmarkDialog::reject()
{
    if (running_) {
        request_stop();
        return;
    }
    QDialog::reject();
}

void SuitabilityBenchmarkDialog::update_retry_controls()
{
    const int row = results_table_ ? results_table_->currentRow() : -1;
    const bool selected = !running_ && row >= 0 && static_cast<size_t>(row) < results_.size();
    if (retry_selected_button_) retry_selected_button_->setEnabled(selected);
    if (try_backend_button_) try_backend_button_->setEnabled(selected);
    if (retry_failed_button_) retry_failed_button_->setEnabled(
        !running_ && !SuitabilityBenchmark::failed_pairs(results_).empty());
}

void SuitabilityBenchmarkDialog::refresh_results_table()
{
    const int selected = results_table_->currentRow();
    const QSignalBlocker blocker(results_table_);
    results_table_->setRowCount(static_cast<int>(results_.size()));
    for (size_t row = 0; row < results_.size(); ++row) {
        const auto& result = results_[row];
        const auto status = [](bool success, double seconds) {
            return success ? QObject::tr("Passed (%1)").arg(QString::fromStdString(format_seconds(seconds)))
                           : QObject::tr("Failed");
        };
        const QStringList cells = {QString::fromStdString(result.label), backend_label(result.backend),
            result.visual ? QStringLiteral("—") : status(result.categorization_success, result.categorization_seconds),
            result.visual ? QStringLiteral("—") : status(result.document_success, result.document_seconds),
            result.visual ? status(result.image_success, result.image_seconds) : QStringLiteral("—")};
        const QString tooltip = QString::fromStdString(result.detail) + QStringLiteral("\n")
            + QObject::tr("Last tested: %1").arg(QString::fromStdString(result.tested_at))
            + QStringLiteral("\n") + QString::fromStdString(result.model_path);
        for (int column = 0; column < cells.size(); ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            item->setToolTip(tooltip);
            if (column >= 2 && cells[column] != QStringLiteral("—")) {
                item->setForeground(cells[column].startsWith(QObject::tr("Failed"))
                    ? QColor("#d73a49") : QColor("#1b9e3c"));
            }
            results_table_->setItem(static_cast<int>(row), column, item);
        }
    }
    if (selected >= 0 && static_cast<size_t>(selected) < results_.size()) results_table_->selectRow(selected);
    else if (!results_.empty()) results_table_->selectRow(0);
    update_retry_controls();
}

void SuitabilityBenchmarkDialog::retry_selected(bool change_backend)
{
    if (running_) return;
    const int row = results_table_->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= results_.size()) return;
    auto request = results_[row];
    if (change_backend) request.backend = backend_combo_->currentData().toString().toStdString();
    start_attempts({std::move(request)}, true);
}

void SuitabilityBenchmarkDialog::retry_failed()
{
    if (running_) return;
    auto requests = SuitabilityBenchmark::failed_pairs(results_);
    if (!requests.empty()) start_attempts(std::move(requests), true);
}

void SuitabilityBenchmarkDialog::start_benchmark()
{
    start_attempts({}, false);
}

void SuitabilityBenchmarkDialog::start_attempts(SuitabilityBenchmark::Results requests, bool retry)
{
    if (running_) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::Owner::Gui;
    metadata.description = "System compatibility check";
    std::string lock_error;
    auto lease = runtime_lock_.try_acquire(std::move(metadata), &lock_error);
    if (!lease) {
        append_line(QObject::tr("The analysis runtime is busy. Wait for the current analysis or compatibility check to finish, then try again."), false);
        return;
    }
    runtime_lease_.emplace(std::move(*lease));
    if (!models_available(settings_)) {
        if (output_view_) {
            output_view_->clear();
        }
        append_line(QObject::tr("No usable local LLM files detected. Download a model or add a valid custom GGUF model to run the benchmark."), false);
        runtime_lease_.reset();
        return;
    }

    if (output_view_) {
        output_view_->clear();
    }
    showing_previous_results_ = false;
    recording_ = true;
    current_report_.clear();
    stop_requested_ = false;
    set_running_state(true);

    inputs_ = BenchmarkInputs{settings_.get_llm_storage_dir(), settings_.get_custom_llms(),
                              settings_.get_sorting_mode()};
    inputs_.requests = std::move(requests);
    inputs_.retry = retry;
    inputs_.backend_mode = backend_combo_->currentData().toString().toStdString();
    pending_results_ = results_;
    pending_results_ready_ = true;
    try {
        worker_ = std::thread(&SuitabilityBenchmarkDialog::run_benchmark_worker, this);
    } catch (const std::exception& ex) {
        append_line(QObject::tr("Could not start the compatibility check: %1")
                        .arg(QString::fromUtf8(ex.what())), false);
        recording_ = false;
        runtime_lease_.reset();
        set_running_state(false);
    }
}

void SuitabilityBenchmarkDialog::run_benchmark_worker()
{
    struct StorageOverrideRestore {
        std::string previous = Utils::get_llm_storage_directory_override();
        ~StorageOverrideRestore() { Utils::set_llm_storage_directory_override(previous); }
    } restore_storage;
    Utils::set_llm_storage_directory_override(inputs_.storage_dir);

    QPointer<SuitabilityBenchmarkDialog> self(this);
    auto post_line = [self](const QString& text) {
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, text]() {
            if (self) {
                self->append_line(text, false);
            }
        }, Qt::QueuedConnection);
    };

    auto post_line_html = [self](const QString& html) {
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, html]() {
            if (self) {
                self->append_line(html, true);
            }
        }, Qt::QueuedConnection);
    };

    auto finish = [self](bool completed) {
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, completed]() {
            if (self) {
                self->finish_benchmark(completed);
            }
        }, Qt::QueuedConnection);
    };

    auto should_stop = [self]() -> bool {
        if (!self) {
            return true;
        }
        return self->stop_requested_.load();
    };

    bool completed = false;
    try {
        post_line(QObject::tr("Starting system compatibility check..."));

        static const std::array<const char*, 4> kBenchmarkEnvKeys = {
            "AI_FILE_SORTER_GPU_BACKEND",
            "LLAMA_ARG_DEVICE",
            "GGML_DISABLE_CUDA",
            "AI_FILE_SORTER_VISUAL_USE_GPU"
        };
        const EnvSnapshot baseline_env = EnvSnapshot::capture(kBenchmarkEnvKeys);
        ScopedEnvRestore env_restore(baseline_env);

        const unsigned int hw_threads = std::max(1u, std::thread::hardware_concurrency());
        post_line(QObject::tr("CPU threads detected: %1").arg(hw_threads));

        const char* backend_env = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
        std::string backend_override = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
        if (backend_env && *backend_env) {
            post_line(QObject::tr("GPU backend override: %1").arg(QString::fromUtf8(backend_env)));
        }

        BenchmarkBackendInfo backend_info;
        bool simulated = false;
#if defined(AI_FILE_SORTER_TEST_BUILD)
        simulated = static_cast<bool>(runner_override_);
        if (simulated) {
            for (const auto& key : available_backends_override_) {
                if (key == "cuda") backend_info.cuda_available = true;
                if (key == "vulkan") backend_info.vulkan_available = true;
                if (key == "metal") backend_info.metal_available = true;
            }
        }
#endif
        if (!simulated) {
        bool cuda_available = false;
        std::optional<BackendMemorySnapshot> vk_memory;
        bool metal_available = false;
        std::optional<BackendMemorySnapshot> metal_memory;

#if defined(__APPLE__)
        metal_available = is_backend_available("Metal");
        post_line(QObject::tr("Metal available: %1")
                      .arg(metal_available ? QObject::tr("yes") : QObject::tr("no")));
        metal_memory = query_backend_memory("metal");
        if (metal_memory.has_value()) {
            post_line(QObject::tr("GPU memory allocation (Metal): %1 free / %2 total")
                          .arg(QString::fromStdString(format_mib(metal_memory->free_bytes)))
                          .arg(QString::fromStdString(format_mib(metal_memory->total_bytes))));
        } else {
            post_line(QObject::tr("GPU memory allocation (Metal): unavailable"));
        }
#else
        cuda_available = Utils::is_cuda_available();
        post_line(QObject::tr("CUDA available: %1")
                      .arg(cuda_available ? QObject::tr("yes") : QObject::tr("no")));
        if (cuda_available) {
            auto cuda_info = Utils::query_cuda_memory();
            if (cuda_info && cuda_info->valid()) {
                QString line = QObject::tr("CUDA memory (allocatable): %1 free / %2 total")
                                   .arg(QString::fromStdString(format_mib(cuda_info->free_bytes)))
                                   .arg(QString::fromStdString(format_mib(cuda_info->total_bytes)));
                if (cuda_info->device_total_bytes > 0) {
                    line += QObject::tr(" (device total: %1)")
                                .arg(QString::fromStdString(format_mib(cuda_info->device_total_bytes)));
                }
                post_line(line);
            }
        }

        vk_memory = query_backend_memory("vulkan");
        if (vk_memory.has_value()) {
            post_line(QObject::tr("GPU memory allocation (Vulkan): %1 free / %2 total")
                          .arg(QString::fromStdString(format_mib(vk_memory->free_bytes)))
                          .arg(QString::fromStdString(format_mib(vk_memory->total_bytes))));
        } else {
            post_line(QObject::tr("GPU memory allocation (Vulkan): unavailable"));
        }
#endif

        backend_info.cuda_available = cuda_available && GgmlRuntimePaths::find_gpu_device("cuda");
        backend_info.vulkan_available = is_backend_available("Vulkan");
        backend_info.vulkan_device = vk_memory ? vk_memory->name : std::string();
        backend_info.metal_available = metal_available;
        backend_info.metal_device = metal_memory ? metal_memory->name : std::string();
        backend_info.blas_label = detect_blas_backend_label();

        }
        const auto available_keys = backend_plan("all", backend_info, {});
        QMetaObject::invokeMethod(self, [self, available_keys]() {
            if (!self) return;
            auto* model = qobject_cast<QStandardItemModel*>(self->backend_combo_->model());
            if (!model) return;
            for (int i = 0; i < self->backend_combo_->count(); ++i) {
                const auto key = self->backend_combo_->itemData(i).toString().toStdString();
                model->item(i)->setEnabled(key == "auto" || key == "all"
                    || std::find(available_keys.begin(), available_keys.end(), key) != available_keys.end());
            }
        }, Qt::QueuedConnection);
        struct TempCleanup {
            std::filesystem::path path;
            ~TempCleanup() {
                if (!path.empty()) {
                    std::error_code ec;
                    std::filesystem::remove_all(path, ec);
                }
            }
        } temp_cleanup{create_temp_dir()};
        const auto& temp_dir = temp_cleanup.path;
        if (temp_dir.empty()) {
            post_line(QObject::tr("Temporary directory setup failed; benchmark sample file creation may fail."));
        }
        const auto candidates = collect_candidates(inputs_.custom_llms);
        // Model identity includes file size and modification time: changed weights need new measurements.
        std::erase_if(pending_results_, [&](const auto& previous) {
            return std::none_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
                return previous.model_key == candidate.model_key && previous.visual == candidate.visual;
            });
        });
        SuitabilityBenchmark::Results jobs;
        std::set<std::pair<std::string, std::string>> scheduled;
        std::set<std::pair<std::string, std::string>> automatic_pairs;
        const auto schedule = [&](const SuitabilityBenchmark::Result& candidate, const std::string& mode) {
            for (const auto& key : backend_plan(mode, backend_info, backend_override)) {
                if (!scheduled.emplace(candidate.model_key, key).second) continue;
                auto request = candidate;
                request.backend = key;
                jobs.push_back(std::move(request));
                if (mode == "auto") automatic_pairs.emplace(candidate.model_key, key);
            }
        };
        if (inputs_.retry) {
            for (const auto& request : inputs_.requests) {
                const auto candidate = std::find_if(candidates.begin(), candidates.end(), [&](const auto& item) {
                    return item.model_key == request.model_key && item.visual == request.visual;
                });
                if (candidate == candidates.end()) {
                    post_line(QObject::tr("The selected model files changed or are no longer available. Run the benchmark again to discover current models."));
                    continue;
                }
                schedule(*candidate, request.backend);
            }
        } else {
            for (const auto& candidate : candidates) schedule(candidate, inputs_.backend_mode);
        }
        if (jobs.empty()) {
            post_line(QObject::tr("No matching model/backend pairs are available to test."));
            finish(false);
            return;
        }
        for (size_t index = 0; index < jobs.size(); ++index) {
            if (should_stop()) break;
            auto result = jobs[index];
            baseline_env.restore();
            ScopedEnvRestore attempt_restore(baseline_env);
            set_env_value("AI_FILE_SORTER_GPU_BACKEND", result.backend);
            set_env_value("LLAMA_ARG_DEVICE", result.backend);
            set_env_value("GGML_DISABLE_CUDA", result.backend == "cpu" ? std::optional<std::string>("1") : std::nullopt);
            set_env_value("AI_FILE_SORTER_VISUAL_USE_GPU", result.backend == "cpu" ? "0" : "1");
            const auto attempt_env = EnvSnapshot::capture(kBenchmarkEnvKeys);
            post_line(QStringLiteral("----"));
            post_line(QObject::tr("Testing %1 on %2")
                .arg(QString::fromStdString(result.label), backend_label(result.backend)));
            try {
                if (!backend_available(result.backend, backend_info)) {
                    result.detail = "The requested backend is not available in this application session.";
                }
#if defined(AI_FILE_SORTER_TEST_BUILD)
                else if (runner_override_) {
                    const auto measured = runner_override_(result);
                    result.categorization_success = measured.categorization_success;
                    result.document_success = measured.document_success;
                    result.image_success = measured.image_success;
                    result.categorization_seconds = measured.categorization_seconds;
                    result.document_seconds = measured.document_seconds;
                    result.image_seconds = measured.image_seconds;
                    result.detail = measured.detail;
                }
#endif
                else if (result.visual) {
                    const auto image = run_image_test(temp_dir, result.visual_id, inputs_.custom_llms,
                                                       backend_info, result.backend);
                    result.image_success = image.success && !image.skipped;
                    result.image_seconds = duration_seconds(image.duration);
                    result.detail = image.detail;
                } else {
                    const LocalModel model{result.label, Utils::utf8_to_path(result.model_path), result.choice};
                    const auto measured = run_text_model_checks({model}, temp_dir, attempt_env,
                        backend_info, post_line, post_line_html, should_stop, result.backend);
                    if (should_stop()) break;
                    result.detail = measured.detail;
                    if (!measured.model_results.empty()) {
                        const auto& timing = measured.model_results.front();
                        result.categorization_success = timing.cat_success;
                        result.document_success = timing.doc_success;
                        result.categorization_seconds = timing.cat_median;
                        result.document_seconds = timing.doc_median;
                    }
                }
            } catch (const std::exception& ex) {
                result.detail = ex.what();
            }
            if (should_stop()) break;
            result.tested_at = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
            post_line(result.success() ? QObject::tr("Model/backend check passed.")
                                      : QObject::tr("Model/backend check failed: %1").arg(QString::fromStdString(result.detail)));
            SuitabilityBenchmark::merge_result(pending_results_, result);
            QMetaObject::invokeMethod(self, [self, result]() {
                if (!self) return;
                SuitabilityBenchmark::merge_result(self->results_, result);
                self->refresh_results_table();
            }, Qt::QueuedConnection);
            if (!result.success() && result.backend != "cpu"
                && automatic_pairs.contains({result.model_key, result.backend})
                && scheduled.emplace(result.model_key, "cpu").second) {
                auto cpu = jobs[index];
                cpu.backend = "cpu";
                jobs.push_back(std::move(cpu));
                post_line(QObject::tr("The GPU attempt failed. Testing this model on CPU as a separate result."));
            }
        }
        if (should_stop()) {
            post_line(QObject::tr("Benchmark stopped."));
            finish(false);
            return;
        }
        post_line(QStringLiteral("----"));
        for (const auto& line : build_pair_summary(pending_results_, inputs_.sorting_mode, &backend_info)) {
            if (line.contains('<')) post_line_html(line);
            else post_line(line);
        }
        completed = true;
    } catch (const std::exception& ex) {
        post_line(QObject::tr("Benchmark failed: %1").arg(QString::fromStdString(ex.what())));
    }

    finish(completed);
}

void SuitabilityBenchmarkDialog::request_stop()
{
    if (!running_) {
        return;
    }
    stop_requested_ = true;
    append_line(QObject::tr("[STOP] Benchmark will stop after the current step is processed."), false);
}

void SuitabilityBenchmarkDialog::append_line(const QString& text, bool is_html)
{
    if (!output_view_) {
        return;
    }

    const QString html = is_html ? text : highlight_figures(text);
    QTextCursor cursor(output_view_->textCursor());
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(html);
    cursor.insertBlock();
    output_view_->setTextCursor(cursor);
    if (auto* scroll = output_view_->verticalScrollBar()) {
        scroll->setValue(scroll->maximum());
    }
    if (recording_) {
        current_report_.push_back(html);
    }
}

void SuitabilityBenchmarkDialog::set_running_state(bool running)
{
    running_ = running;
    if (backend_combo_) backend_combo_->setEnabled(!running);
    update_retry_controls();
    if (progress_bar_) {
        progress_bar_->setVisible(running);
    }
    if (run_button_) {
        run_button_->setEnabled(!running);
    }
    if (stop_button_) {
        stop_button_->setEnabled(running);
    }
    if (close_button_) {
        close_button_->setEnabled(!running);
    }
}

void SuitabilityBenchmarkDialog::finish_benchmark(bool completed)
{
    if (worker_.joinable()) {
        worker_.join();
    }
    if (pending_results_ready_) {
        results_ = std::move(pending_results_);
        pending_results_ready_ = false;
    }
    refresh_results_table();
    recording_ = false;
    const std::string timestamp = format_timestamp(std::chrono::system_clock::now());
    settings_.set_benchmark_last_run(timestamp);
    settings_.set_benchmark_last_report(current_report_.join('\n').toStdString());
    settings_.set_benchmark_results_json(SuitabilityBenchmark::serialize_results(results_));
    if (completed) {
        settings_.set_suitability_benchmark_completed(true);
    }
    settings_.save();
    runtime_lease_.reset();
    set_running_state(false);
}
