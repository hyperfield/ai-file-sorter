#include "SuitabilityBenchmarkDialog.hpp"

#include "DocumentTextAnalyzer.hpp"
#include "ILLMClient.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "LlmCatalog.hpp"
#include "LocalLLMClient.hpp"
#include "Settings.hpp"
#include "Types.hpp"
#include "Utils.hpp"
#include "ggml-backend.h"

#include <QCloseEvent>
#include <QColor>
#include <QEvent>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QPainter>
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
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
struct VisualLlmPaths {
    std::filesystem::path model_path;
    std::filesystem::path mmproj_path;
};

std::optional<VisualLlmPaths> resolve_visual_llm_paths(std::string* error);

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
        const double parsed = std::stod(value, &idx);
        if (idx == 0) {
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
    case LLMChoice::Local_3b:
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
    default:
        return PerfThresholds{
            read_env_double("BENCH_CAT_DEFAULT_OPTIMAL_MAX", 5.0),
            read_env_double("BENCH_CAT_DEFAULT_ACCEPTABLE_MAX", 10.0)};
    }
}

PerfThresholds thresholds_for_document_choice(LLMChoice choice)
{
    switch (choice) {
    case LLMChoice::Local_3b:
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
    if (seconds <= thresholds.acceptable_max) {
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

struct DefaultModel {
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

struct EnvEntry {
    std::string key;
    std::optional<std::string> value;
};

class EnvSnapshot {
public:
    static EnvSnapshot capture(const std::array<const char*, 3>& keys)
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

BackendTarget resolve_backend_target(std::string_view override_value)
{
    if (override_value == "cpu") {
        return {"cpu", QObject::tr("CPU").toStdString()};
    }
    if (override_value == "metal") {
        return {"metal", QObject::tr("Metal").toStdString()};
    }
    if (override_value == "cuda") {
        return {"cuda", QObject::tr("CUDA").toStdString()};
    }
    if (override_value == "vulkan") {
        return {"vulkan", QObject::tr("Vulkan").toStdString()};
    }
    if (override_value == "auto") {
#if defined(__APPLE__)
        return {"metal", QObject::tr("Metal (auto)").toStdString()};
#else
        return {"vulkan", QObject::tr("Vulkan (auto)").toStdString()};
#endif
    }
    if (!override_value.empty()) {
        return {"auto",
                QObject::tr("%1 (auto)").arg(QString::fromStdString(std::string(override_value))).toStdString()};
    }
#if defined(__APPLE__)
    return {"metal", QObject::tr("Metal (auto)").toStdString()};
#else
    return {"vulkan", QObject::tr("Vulkan (auto)").toStdString()};
#endif
}

std::string format_backend_label(std::string_view name)
{
    if (name.empty()) {
        return QObject::tr("Auto").toStdString();
    }
    const std::string lowered = to_lower_copy(std::string(name));
    if (lowered == "vulkan") {
        return QObject::tr("Vulkan").toStdString();
    }
    if (lowered == "cuda") {
        return QObject::tr("CUDA").toStdString();
    }
    if (lowered == "metal") {
        return QObject::tr("Metal").toStdString();
    }
    if (lowered == "cpu") {
        return QObject::tr("CPU").toStdString();
    }
    return std::string(name);
}

std::optional<std::filesystem::path> resolve_default_model_path(const char* env_var)
{
    const char* url = std::getenv(env_var);
    if (!url || !*url) {
        return std::nullopt;
    }
    try {
        std::filesystem::path path = Utils::make_default_path_to_file_from_download_url(url);
        if (std::filesystem::exists(path)) {
            return path;
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

std::vector<DefaultModel> collect_default_models()
{
    std::vector<DefaultModel> models;
    std::unordered_set<std::string> seen;
    for (const auto& entry : default_llm_entries()) {
        auto path = resolve_default_model_path(entry.url_env);
        if (!path) {
            continue;
        }
        const std::string path_key = path->string();
        if (!seen.insert(path_key).second) {
            continue;
        }
        models.push_back(DefaultModel{default_llm_label(entry).toStdString(), *path, entry.choice});
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
    static bool loaded = false;
    if (loaded) {
        return;
    }

    const char* ggml_dir = std::getenv("AI_FILE_SORTER_GGML_DIR");
    if (ggml_dir && *ggml_dir) {
        ggml_backend_load_all_from_path(ggml_dir);
    } else {
        ggml_backend_load_all();
    }
    loaded = true;
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
        if (!case_insensitive_contains(reg_name ? reg_name : "", backend_name)) {
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
    const std::string name(backend_name);
    ggml_backend_reg_t reg = ggml_backend_reg_by_name(name.c_str());
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

QString build_backend_note(const BackendTarget& target,
                           const BenchmarkBackendInfo& info,
                           bool gpu_fallback)
{
    if (target.key == "cpu") {
        return build_cpu_backend_note(QString(), info.blas_label);
    }

    if (gpu_fallback) {
        QString reason;
        if (target.key == "vulkan" && !info.vulkan_available) {
            reason = QObject::tr("GPU via Vulkan unavailable");
        } else if (target.key == "cuda" && !info.cuda_available) {
            reason = QObject::tr("GPU via CUDA unavailable");
            if (!info.vulkan_available) {
                reason += QStringLiteral("; ") + QObject::tr("Vulkan unavailable");
            }
        } else if (target.key == "metal" && !info.metal_available) {
            reason = QObject::tr("GPU via Metal unavailable");
        } else {
            reason = QObject::tr("GPU init failed");
        }
        return build_cpu_backend_note(reason, info.blas_label);
    }

    return build_gpu_backend_note(target);
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

bool has_visual_llm_files()
{
    return resolve_visual_llm_paths(nullptr).has_value();
}

bool has_any_llm_available()
{
    return !collect_default_models().empty() || has_visual_llm_files();
}

std::filesystem::path create_temp_dir()
{
    std::error_code ec;
    std::filesystem::path base = std::filesystem::temp_directory_path(ec);
    if (ec || base.empty()) {
        base = std::filesystem::current_path();
    }
    base /= "aifs-benchmark";
    std::filesystem::create_directories(base, ec);
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::filesystem::path run_dir = base / ("run-" + std::to_string(stamp));
    std::filesystem::create_directories(run_dir, ec);
    return run_dir;
}

bool write_sample_document(const std::filesystem::path& path)
{
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
    QImage image(96, 96, QImage::Format_RGB32);
    image.fill(QColor(30, 120, 200));
    QPainter painter(&image);
    painter.fillRect(QRect(18, 18, 60, 60), QColor(240, 200, 60));
    painter.end();
    return image.save(QString::fromStdString(path.string()), "PNG");
}

std::optional<std::filesystem::path> resolve_mmproj_path(const std::filesystem::path& primary)
{
    if (std::filesystem::exists(primary)) {
        return primary;
    }

    const auto llm_dir = std::filesystem::path(Utils::get_default_llm_destination());
    static const char* kAltMmprojNames[] = {
        "mmproj-model-f16.gguf",
        "llava-v1.6-mistral-7b-mmproj-f16.gguf"
    };
    for (const char* alt_name : kAltMmprojNames) {
        const auto candidate = llm_dir / alt_name;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

std::optional<VisualLlmPaths> resolve_visual_llm_paths(std::string* error)
{
    const char* model_url = std::getenv("LLAVA_MODEL_URL");
    const char* mmproj_url = std::getenv("LLAVA_MMPROJ_URL");
    if (!model_url || !*model_url || !mmproj_url || !*mmproj_url) {
        if (error) {
            *error = "Missing visual LLM download URLs.";
        }
        return std::nullopt;
    }

    std::filesystem::path model_path;
    std::filesystem::path mmproj_primary;
    try {
        model_path = std::filesystem::path(Utils::make_default_path_to_file_from_download_url(model_url));
        mmproj_primary = std::filesystem::path(Utils::make_default_path_to_file_from_download_url(mmproj_url));
    } catch (...) {
        if (error) {
            *error = "Failed to resolve visual LLM file paths.";
        }
        return std::nullopt;
    }

    if (!std::filesystem::exists(model_path)) {
        if (error) {
            *error = "Visual LLM model file is missing.";
        }
        return std::nullopt;
    }

    auto mmproj_path = resolve_mmproj_path(mmproj_primary);
    if (!mmproj_path) {
        if (error) {
            *error = "Visual LLM mmproj file is missing.";
        }
        return std::nullopt;
    }

    return VisualLlmPaths{model_path, *mmproj_path};
}

bool should_use_visual_gpu()
{
    const char* backend = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
    if (!backend || !*backend) {
        return true;
    }
    std::string lowered = backend;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered != "cpu";
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
    std::string detail;
    std::chrono::steady_clock::duration duration{};
};

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
    };
    std::vector<ModelPerf> model_results;
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

StepResult run_image_test(const std::filesystem::path& temp_dir)
{
    StepResult result;
#if defined(AI_FILE_SORTER_HAS_MTMD)
    std::string visual_error;
    auto visual_paths = resolve_visual_llm_paths(&visual_error);
    if (!visual_paths) {
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
        LlavaImageAnalyzer::Settings settings;
        settings.use_gpu = should_use_visual_gpu();
        LlavaImageAnalyzer analyzer(visual_paths->model_path, visual_paths->mmproj_path, settings);
        const auto analysis = analyzer.analyze(image_path);
        result.success = !analysis.suggested_name.empty();
        if (!result.success) {
            result.detail = "Empty suggestion.";
        }
    } catch (const std::exception& ex) {
        result.detail = ex.what();
    }
    result.duration = std::chrono::steady_clock::now() - start;
#else
    result.skipped = true;
    result.detail = "Visual LLM support is not available in this build.";
#endif
    return result;
}

TextModelOutcome run_text_model_checks(const std::vector<DefaultModel>& models,
                                       const std::filesystem::path& temp_dir,
                                       const EnvSnapshot& baseline_env,
                                       const BenchmarkBackendInfo& backend_info,
                                       const std::function<void(const QString&)>& post_line,
                                       const std::function<void(const QString&)>& post_line_html,
                                       const std::function<bool()>& should_stop)
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
                post_line_html(QObject::tr("Default model: %1").arg(bold_llm_label(model.label)));
            } else {
                post_line(QObject::tr("Default model: %1").arg(QString::fromStdString(model.label)));
            }
        }
        try {
            LocalLLMClient client(model.path.string());

            StepResult cat_warm;
            StepResult cat_init;
            std::vector<StepResult> cat_runs;
            StepResult doc_warm;
            StepResult doc_init;
            std::vector<StepResult> doc_runs;
            bool status_fallback = false;
            client.set_status_callback([&status_fallback](LocalLLMClient::Status status) {
                if (status == LocalLLMClient::Status::GpuFallbackToCpu) {
                    status_fallback = true;
                }
            });

            const std::string backend_before_cat = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
            const BackendTarget cat_target = resolve_backend_target(backend_before_cat);
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
            const BackendTarget doc_target = resolve_backend_target(backend_before_doc);
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

            outcome.categorization_ok = outcome.categorization_ok && cat_success;
            outcome.document_ok = outcome.document_ok && doc_success;

            std::vector<double> cat_per_seconds;
            cat_per_seconds.reserve(cat_runs.size());
            for (const auto& run : cat_runs) {
                cat_per_seconds.push_back(duration_seconds(run.duration));
            }
            const double cat_median = median_seconds(cat_per_seconds);
            const PerfClass cat_perf = classify_perf(cat_median, thresholds_for_choice(model.choice));
            outcome.categorization_perf = outcome.categorization_perf.has_value()
                ? std::optional<PerfClass>(worst_perf(*outcome.categorization_perf, cat_perf))
                : std::optional<PerfClass>(cat_perf);

            std::vector<double> doc_per_seconds;
            doc_per_seconds.reserve(doc_runs.size());
            for (const auto& run : doc_runs) {
                doc_per_seconds.push_back(duration_seconds(run.duration));
            }
            const double doc_median = median_seconds(doc_per_seconds);
            const PerfClass doc_perf = classify_perf(doc_median, thresholds_for_document_choice(model.choice));
            outcome.document_perf = outcome.document_perf.has_value()
                ? std::optional<PerfClass>(worst_perf(*outcome.document_perf, doc_perf))
                : std::optional<PerfClass>(doc_perf);

            outcome.model_results.push_back(TextModelOutcome::ModelPerf{
                model.label,
                model.choice,
                cat_perf,
                doc_perf,
                cat_median,
                doc_median
            });

            if (post_line) {
                post_line(QObject::tr("Categorization: %1")
                              .arg(cat_success ? QObject::tr("done") : QObject::tr("failed")));
                if (post_line_html) {
                    post_line_html(QObject::tr("    Warm-up: %1")
                                      .arg(colored_seconds(duration_seconds(cat_warm.duration), PerfClass::Optimal)));
                    post_line_html(QObject::tr("    Init: %1")
                                      .arg(colored_seconds(duration_seconds(cat_init.duration), PerfClass::Optimal)));
                    post_line_html(QObject::tr("    Per-item (median of %1): %2")
                                      .arg(cat_per_seconds.size())
                                      .arg(colored_seconds(cat_median, cat_perf)));
                    post_line_html(QObject::tr("    Per-item runs: %1")
                                      .arg(colored_seconds_list(
                                          cat_per_seconds,
                                          [model](double value) {
                                              return classify_perf(value, thresholds_for_choice(model.choice));
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
                                      .arg(colored_seconds(duration_seconds(doc_warm.duration), PerfClass::Optimal)));
                    post_line_html(QObject::tr("    Init: %1")
                                      .arg(colored_seconds(duration_seconds(doc_init.duration), PerfClass::Optimal)));
                    post_line_html(QObject::tr("    Per-item (median of %1): %2")
                                      .arg(doc_per_seconds.size())
                                      .arg(colored_seconds(doc_median, doc_perf)));
                    post_line_html(QObject::tr("    Per-item runs: %1")
                                      .arg(colored_seconds_list(
                                          doc_per_seconds,
                                          [model](double value) {
                                              return classify_perf(value, thresholds_for_document_choice(model.choice));
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
                               const StepResult& image)
{
    QStringList lines;
    lines << QStringLiteral("<span style=\"color:#1f6feb; font-weight:700;\">%1</span>")
                 .arg(QObject::tr("Result"));

    if (text_models.skipped) {
        lines << QObject::tr("Categorization speed: unavailable");
        lines << QObject::tr("Document analysis speed: unavailable");
    } else {
        PerfClass cat_perf = PerfClass::TooLong;
        PerfClass doc_perf = PerfClass::TooLong;
        for (const auto& entry : text_models.model_results) {
            if (perf_rank(entry.cat_perf) < perf_rank(cat_perf)) {
                cat_perf = entry.cat_perf;
            }
            if (perf_rank(entry.doc_perf) < perf_rank(doc_perf)) {
                doc_perf = entry.doc_perf;
            }
        }
        lines << QObject::tr("Categorization speed: %1").arg(colored_perf_label(cat_perf));
        lines << QObject::tr("Document analysis speed: %1").arg(colored_perf_label(doc_perf));
    }

    if (image.skipped) {
        lines << QObject::tr("Image analysis speed: unavailable");
    } else if (image.success) {
        const double seconds = duration_seconds(image.duration);
        const PerfClass image_perf = classify_perf(seconds, image_thresholds());
        lines << QObject::tr("Image analysis speed: %1").arg(colored_perf_label(image_perf));
    } else {
        lines << QObject::tr("Image analysis speed: %1").arg(colored_perf_label(PerfClass::TooLong));
    }

    QStringList recommended_labels;
    if (!text_models.model_results.empty()) {
        std::vector<const TextModelOutcome::ModelPerf*> optimal;
        std::vector<const TextModelOutcome::ModelPerf*> acceptable;
        std::vector<const TextModelOutcome::ModelPerf*> quite_long;
        for (const auto& entry : text_models.model_results) {
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
    lines << QString();
    lines << QStringLiteral("<span style=\"color:#1f6feb;\">%1</span>")
                 .arg(QObject::tr("You can toggle LLMs in Settings -> Select LLM").toHtmlEscaped());
    return lines;
}
} // namespace

SuitabilityBenchmarkDialog::SuitabilityBenchmarkDialog(Settings& settings,
                                                       QWidget* parent)
    : QDialog(parent)
    , settings_(settings)
{
    resize(820, 560);
    setup_ui();
    retranslate_ui();
    load_previous_results();
}

SuitabilityBenchmarkDialog::~SuitabilityBenchmarkDialog()
{
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SuitabilityBenchmarkDialog::setup_ui()
{
    auto* layout = new QVBoxLayout(this);

    intro_label_ = new QLabel(this);
    intro_label_->setTextFormat(Qt::RichText);
    intro_label_->setWordWrap(true);
    layout->addWidget(intro_label_);

    output_view_ = new QTextEdit(this);
    output_view_->setReadOnly(true);
    output_view_->setAcceptRichText(true);
    output_view_->setLineWrapMode(QTextEdit::WidgetWidth);
    layout->addWidget(output_view_, 1);

    progress_bar_ = new QProgressBar(this);
    progress_bar_->setVisible(false);
    progress_bar_->setRange(0, 0);
    layout->addWidget(progress_bar_);

    auto* button_layout = new QHBoxLayout();
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
}

void SuitabilityBenchmarkDialog::retranslate_ui()
{
    setWindowTitle(QObject::tr("Compatability Benchmark"));
    if (intro_label_) {
        const QString intro_main = QObject::tr("Run a quick performance check to estimate how image analysis, document analysis, and file categorization will perform on your system.");
        const QString intro_warning = QObject::tr("It is recommended to quit any CPU- and GPU-intensive applications before running this test.");
        intro_label_->setText(QStringLiteral("%1<br><br><span style=\"color:#d73a49; font-weight:600;\">%2</span>")
                                  .arg(intro_main.toHtmlEscaped(), intro_warning.toHtmlEscaped()));
    }
    if (run_button_) {
        run_button_->setText(QObject::tr("Run benchmark"));
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

void SuitabilityBenchmarkDialog::start_benchmark()
{
    if (running_) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    if (!has_any_llm_available()) {
        if (output_view_) {
            output_view_->clear();
        }
        append_line(QObject::tr("No downloaded LLM files detected. Download a categorization or visual model to run the benchmark."), false);
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

    worker_ = std::thread(&SuitabilityBenchmarkDialog::run_benchmark_worker, this);
}

void SuitabilityBenchmarkDialog::run_benchmark_worker()
{
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

    auto finish = [self]() {
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self]() {
            if (self) {
                self->finish_benchmark();
            }
        }, Qt::QueuedConnection);
    };

    auto should_stop = [self]() -> bool {
        if (!self) {
            return true;
        }
        return self->stop_requested_.load();
    };

    try {
        post_line(QObject::tr("Starting system compatibility check..."));

        static const std::array<const char*, 3> kBenchmarkEnvKeys = {
            "AI_FILE_SORTER_GPU_BACKEND",
            "LLAMA_ARG_DEVICE",
            "GGML_DISABLE_CUDA"
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

        BenchmarkBackendInfo backend_info;
        backend_info.cuda_available = cuda_available;
        backend_info.vulkan_available = vk_memory.has_value();
        backend_info.vulkan_device = vk_memory ? vk_memory->name : std::string();
        backend_info.metal_available = metal_available;
        backend_info.metal_device = metal_memory ? metal_memory->name : std::string();
        backend_info.blas_label = detect_blas_backend_label();

        const auto temp_dir = create_temp_dir();
        const std::vector<DefaultModel> default_models = collect_default_models();
        if (default_models.empty()) {
            post_line(QObject::tr("No default models downloaded; skipping categorization and document checks."));
        } else {
            post_line(QObject::tr("Default models detected: %1").arg(default_models.size()));
        }
        post_line(QStringLiteral("----"));

        const TextModelOutcome text_outcome = run_text_model_checks(default_models,
                                                                    temp_dir,
                                                                    baseline_env,
                                                                    backend_info,
                                                                    post_line,
                                                                    post_line_html,
                                                                    should_stop);

        if (should_stop()) {
            post_line(QObject::tr("Benchmark stopped."));
            finish();
            return;
        }

        post_line(QStringLiteral("----"));
        post_line(QObject::tr("Running image analysis test..."));
        baseline_env.restore();
        ScopedEnvRestore image_env_restore(baseline_env);
        if (should_stop()) {
            post_line(QObject::tr("Benchmark stopped."));
            finish();
            return;
        }
        StepResult image_result = run_image_test(temp_dir);

        if (image_result.skipped) {
            const QString detail = image_result.detail.empty()
                ? QObject::tr("unavailable")
                : QString::fromStdString(image_result.detail);
            post_line(QObject::tr("Image analysis: skipped (%1)").arg(detail));
        } else {
            const double seconds = duration_seconds(image_result.duration);
            const PerfClass image_perf = classify_perf(seconds, image_thresholds());
            post_line(QObject::tr("Image analysis: %1")
                          .arg(image_result.success ? QObject::tr("done") : QObject::tr("failed")));
            post_line_html(QObject::tr("    Time: %1")
                              .arg(colored_seconds(seconds, image_perf)));
            if (!image_result.success && !image_result.detail.empty()) {
                post_line(QObject::tr("Details: %1").arg(QString::fromStdString(image_result.detail)));
            }
        }

        if (should_stop()) {
            post_line(QObject::tr("Benchmark stopped."));
            finish();
            return;
        }

        if (!image_result.skipped) {
            std::string visual_backend = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
            if (visual_backend.empty()) {
                visual_backend = read_env_lower("LLAMA_ARG_DEVICE");
            }

            QString backend_note;
            if (!should_use_visual_gpu()) {
                backend_note = build_cpu_backend_note(QObject::tr("GPU disabled by backend override"), std::nullopt);
            } else if (case_insensitive_contains(visual_backend, "vulkan") &&
                       !backend_info.vulkan_available) {
                backend_note = build_cpu_backend_note(QObject::tr("GPU via Vulkan unavailable"), std::nullopt);
            } else if (case_insensitive_contains(visual_backend, "metal") &&
                       !backend_info.metal_available) {
                backend_note = build_cpu_backend_note(QObject::tr("GPU via Metal unavailable"), std::nullopt);
            } else {
                BackendTarget visual_target = resolve_backend_target(visual_backend);
                if (visual_target.key == "cpu") {
                    backend_note = build_cpu_backend_note(QObject::tr("GPU disabled by backend override"), std::nullopt);
                } else {
                    if (!visual_backend.empty()) {
                        visual_target.label = format_backend_label(visual_backend);
                    }
                    backend_note = build_gpu_backend_note(visual_target);
                }
            }

            post_line(QObject::tr("Backend used (image analysis): %1")
                          .arg(backend_note));
        }

        post_line(QStringLiteral("----"));
        const QStringList result_lines = build_result_lines(text_outcome, image_result);
        for (const QString& line : result_lines) {
            if (line.contains('<')) {
                post_line_html(line);
            } else {
                post_line(line);
            }
        }

        std::error_code cleanup_error;
        std::filesystem::remove_all(temp_dir, cleanup_error);
    } catch (const std::exception& ex) {
        post_line(QObject::tr("Benchmark failed: %1").arg(QString::fromStdString(ex.what())));
    }

    finish();
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

void SuitabilityBenchmarkDialog::finish_benchmark()
{
    recording_ = false;
    const std::string timestamp = format_timestamp(std::chrono::system_clock::now());
    settings_.set_benchmark_last_run(timestamp);
    settings_.set_benchmark_last_report(current_report_.join('\n').toStdString());
    settings_.set_suitability_benchmark_completed(true);
    settings_.save();
    set_running_state(false);
}
