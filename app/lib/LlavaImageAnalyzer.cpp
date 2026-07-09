#include "LlavaImageAnalyzer.hpp"

#include "Logger.hpp"
#include "LlamaModelParams.hpp"
#include "Utils.hpp"

#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <climits>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef AI_FILE_SORTER_HAS_MTMD
#include "ggml-backend.h"
#include "gguf.h"
#include "llama.h"
#include "mtmd.h"
#include "mtmd-helper.h"
#endif

#ifdef AI_FILE_SORTER_HAS_MTMD
extern "C" {
#if defined(AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
typedef void (*mtmd_progress_callback_t)(const char* name,
                                         int32_t current_batch,
                                         int32_t total_batches,
                                         void* user_data);
#if !defined(_WIN32)
MTMD_API void mtmd_helper_set_progress_callback(mtmd_progress_callback_t callback,
                                                void* user_data);
#endif
#endif
#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK) && !defined(_WIN32)
MTMD_API void mtmd_helper_log_set(ggml_log_callback log_callback, void* user_data);
#endif
}
#endif

namespace {
constexpr size_t kMaxFilenameWords = 3;
constexpr size_t kMaxFilenameLength = 50;
constexpr int32_t kDefaultVisualCpuBatchSize = 512;
constexpr int32_t kDefaultVisualGpuBatchSize = 512;
constexpr int32_t kDefaultMetalVisualBatchSize = 1024;
constexpr int32_t kVisualBatchRetrySize = 256;
constexpr int32_t kVisualBatchEmergencySize = 128;
constexpr int32_t kVisualReducedContextTokens = 2048;
constexpr int32_t kVisualMinimumContextTokens = 1024;
constexpr int32_t kVisualGpuLayerRetryScaleNumerator = 3;
constexpr int32_t kVisualGpuLayerRetryScaleDenominator = 4;
constexpr int32_t kMinimumVisualGpuLayerRetryCount = 1;
constexpr std::size_t kEstimatedPromptBufferBytes = 4096;
constexpr std::size_t kGeneratedResponseReserveBytes = 256;
constexpr std::size_t kTokenPieceBufferBytes = 256;
constexpr double kBytesPerMiB = 1024.0 * 1024.0;
constexpr double kVisualSafetyReserveFraction = 0.05;
constexpr double kVisualMinimumSafetyReserveBytes = 192.0 * kBytesPerMiB;
constexpr double kVisualBudgetFallbackFraction = 0.75;
constexpr double kVisualMaxApproxFreeBudgetFraction = 0.98;
constexpr double kVisualMaxUsableTotalBudgetFraction = 0.90;
constexpr double kVisualMinApproxFreeBudgetFraction = 0.45;
constexpr double kVisualLayerOverheadFactor = 1.08;
constexpr int32_t kVisualTierHeuristicReconciliationWindowLayers = 4;

#if defined(AI_FILE_SORTER_HAS_MTMD) && defined(_WIN32)
FARPROC resolve_mtmd_helper(const char* name) {
    HMODULE module = GetModuleHandleW(L"mtmd.dll");
    if (!module) {
        return nullptr;
    }
    return GetProcAddress(module, name);
}
#endif

#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
bool set_mtmd_log_callback(ggml_log_callback callback, void* user_data) {
#if defined(_WIN32)
    using LogSetFn = void (*)(ggml_log_callback, void*);
    auto* fn = reinterpret_cast<LogSetFn>(resolve_mtmd_helper("mtmd_helper_log_set"));
    if (!fn) {
        return false;
    }
    fn(callback, user_data);
    return true;
#else
    mtmd_helper_log_set(callback, user_data);
    return true;
#endif
}
#endif

#if defined(AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
bool set_mtmd_progress_callback(mtmd_progress_callback_t callback, void* user_data) {
#if defined(_WIN32)
    using ProgressSetFn = void (*)(mtmd_progress_callback_t, void*);
    auto* fn = reinterpret_cast<ProgressSetFn>(resolve_mtmd_helper("mtmd_helper_set_progress_callback"));
    if (!fn) {
        return false;
    }
    fn(callback, user_data);
    return true;
#else
    mtmd_helper_set_progress_callback(callback, user_data);
    return true;
#endif
}
#endif

struct PromptRequest {
    std::string system_prompt;
    std::string user_prompt;
};

PromptRequest build_description_request(VisualPromptPolicy policy) {
    switch (policy) {
    case VisualPromptPolicy::LegacyLlava: {
        std::ostringstream oss;
        oss << "Please provide a detailed description of this image, focusing on the main subject "
            << "and any important details.\n"
            << "Image: <__media__>\n"
            << "Description:";
        return {"", oss.str()};
    }
    case VisualPromptPolicy::StructuredVisionInstruct: {
        std::ostringstream oss;
        oss << "Analyze this image for file organization.\n"
            << "<__media__>\n"
            << "Return one concise description that captures the file type or scene, the main subject, "
            << "the setting or context, any visible text, and concrete details that would help with "
            << "categorization or naming.\n"
            << "Output only the description.";
        return {
            "You describe images for file organization and retrieval. Focus on concrete details that "
            "help with sorting and naming.",
            oss.str()
        };
    }
    }

    return {"", ""};
}

PromptRequest build_filename_request(VisualPromptPolicy policy, std::string_view description) {
    switch (policy) {
    case VisualPromptPolicy::LegacyLlava: {
        std::ostringstream oss;
        oss << "Based on the description below, generate a specific and descriptive filename for the image.\n"
            << "Limit the filename to a maximum of 3 words. Use nouns and avoid starting with verbs like "
            << "'depicts', 'shows', 'presents', etc.\n"
            << "Do not include any data type words like 'image', 'jpg', 'png', etc. Use only letters and "
            << "connect words with underscores.\n\n"
            << "Description: " << description << "\n\n"
            << "Example:\n"
            << "Description: A photo of a sunset over the mountains.\n"
            << "Filename: sunset_over_mountains\n\n"
            << "Now generate the filename.\n\n"
            << "Output only the filename, without any additional text.\n\n"
            << "Filename:";
        return {"", oss.str()};
    }
    case VisualPromptPolicy::StructuredVisionInstruct: {
        std::ostringstream oss;
        oss << "Create a short filename stem from this image description.\n"
            << "Description: " << description << "\n\n"
            << "Rules:\n"
            << "- maximum 3 words\n"
            << "- lowercase letters only\n"
            << "- join words with underscores\n"
            << "- prefer concrete nouns\n"
            << "- do not include a file extension\n"
            << "- do not include quotes or extra commentary\n\n"
            << "Output only the filename stem.";
        return {
            "You generate short filesystem-safe filename stems from image descriptions.",
            oss.str()
        };
    }
    }

    return {"", ""};
}

#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
bool is_mtmd_prompt_log_line(std::string_view line) {
    return line.starts_with("add_text:");
}
#endif

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::optional<bool> read_env_bool(const char* key) {
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }

    std::string lowered = to_lower_copy(value);
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<std::string> read_env_value(const char* key) {
    const char* value = std::getenv(key);
    if (!value) {
        return std::nullopt;
    }
    return std::string(value);
}

void set_env_value(const char* key, const std::optional<std::string>& value) {
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

class ScopedEnvValue {
public:
    ScopedEnvValue(const char* key, std::optional<std::string> value)
        : key_(key), original_(read_env_value(key)) {
        set_env_value(key_, value);
    }

    ~ScopedEnvValue() {
        set_env_value(key_, original_);
    }

    ScopedEnvValue(const ScopedEnvValue&) = delete;
    ScopedEnvValue& operator=(const ScopedEnvValue&) = delete;

private:
    const char* key_;
    std::optional<std::string> original_;
};

std::optional<int> read_env_int(const char* key) {
    const char* value = std::getenv(key);
    if (!value || *value == '\0') {
        return std::nullopt;
    }

    char* end_ptr = nullptr;
    errno = 0;
    long parsed = std::strtol(value, &end_ptr, 10);
    if (end_ptr == value || *end_ptr != '\0' || errno == ERANGE) {
        return std::nullopt;
    }
    if (parsed < INT_MIN || parsed > INT_MAX) {
        return std::nullopt;
    }

    return static_cast<int>(parsed);
}

std::optional<int> resolve_visual_gpu_layer_override() {
    if (const auto visual_override = read_env_int("AI_FILE_SORTER_VISUAL_N_GPU_LAYERS")) {
        return visual_override;
    }
    return read_env_int("LLAVA_N_GPU_LAYERS");
}

std::string read_env_lower(const char* key) {
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return {};
    }
    return to_lower_copy(value);
}

std::string resolve_backend_name() {
    std::string backend = read_env_lower("AI_FILE_SORTER_GPU_BACKEND");
    if (backend.empty()) {
        backend = read_env_lower("LLAMA_ARG_DEVICE");
    }
#ifdef GGML_USE_METAL
    if (backend.empty()) {
        backend = "metal";
    }
#endif
    return backend;
}

std::string trim_copy(const std::string& value) {
    auto result = value;
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), not_space));
    result.erase(std::find_if(result.rbegin(), result.rend(), not_space).base(), result.end());
    return result;
}

QString sanitize_utf8_text(const std::string& value) {
    QString cleaned = QString::fromUtf8(value.c_str());
    cleaned.remove(QChar::ReplacementCharacter);
    return cleaned.normalized(QString::NormalizationForm_C);
}

double elapsed_ms(const std::chrono::steady_clock::time_point start,
                  const std::chrono::steady_clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

std::vector<std::string> split_words(const QString& value) {
    std::vector<std::string> words;
    QString current;
    for (const QChar ch : value) {
        if (ch.isLetterOrNumber()) {
            current.append(ch.toLower());
        } else if (!current.isEmpty()) {
            words.emplace_back(current.toUtf8().toStdString());
            current.clear();
        }
    }
    if (!current.isEmpty()) {
        words.emplace_back(current.toUtf8().toStdString());
    }
    return words;
}

const std::unordered_set<std::string> kStopwords = {
    "a", "an", "and", "are", "as", "at", "based", "be", "by", "category", "describes",
    "description", "depicts", "details", "document", "file", "filename", "for", "from",
    "gif", "has", "image", "in", "is", "it", "jpeg", "jpg", "of", "on", "only",
    "photo", "picture", "png", "shows", "the", "this", "to", "txt", "unknown", "with"
};

bool case_insensitive_contains(std::string_view text, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    std::string text_lower(text);
    std::string needle_lower(needle);
    std::transform(text_lower.begin(), text_lower.end(), text_lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(needle_lower.begin(), needle_lower.end(), needle_lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text_lower.find(needle_lower) != std::string::npos;
}

int32_t resolve_default_visual_batch_size(bool gpu_enabled, std::string_view backend_name) {
    if (!gpu_enabled) {
        return kDefaultVisualCpuBatchSize;
    }
    if (case_insensitive_contains(backend_name, "metal")) {
        return kDefaultMetalVisualBatchSize;
    }
#if defined(_WIN32)
    // Windows GPU drivers are more sensitive to an initial oversized context
    // allocation, so keep image analysis at the proven GPU-safe batch size.
    return kDefaultVisualGpuBatchSize;
#else
    if (case_insensitive_contains(backend_name, "vulkan")) {
        return kDefaultVisualGpuBatchSize;
    }
    return kDefaultVisualGpuBatchSize;
#endif
}

constexpr size_t kVisualEvalHeadroomBytes = 640ULL * 1024ULL * 1024ULL;
constexpr std::size_t kVisualMetadataScanBytes = 8ULL * 1024ULL * 1024ULL;

struct BackendMemoryInfo {
    size_t free_bytes = 0;
    size_t total_bytes = 0;
    std::string name;
};

std::vector<int32_t> visual_eval_retry_batches(int32_t current_batch) {
    std::vector<int32_t> batches;
    if (current_batch > kDefaultVisualGpuBatchSize) {
        batches.push_back(kDefaultVisualGpuBatchSize);
    }
    if (current_batch > kVisualBatchRetrySize) {
        batches.push_back(kVisualBatchRetrySize);
    }
    if (current_batch > kVisualBatchEmergencySize) {
        batches.push_back(kVisualBatchEmergencySize);
    }
    return batches;
}

void append_unique_positive_visual_retry_layers(std::vector<int32_t>& candidates,
                                                int32_t layers) {
    if (layers <= 0) {
        return;
    }
    if (!candidates.empty() && candidates.back() == layers) {
        return;
    }
    candidates.push_back(layers);
}

int32_t reduced_visual_retry_layers(int32_t current_layers) {
    if (current_layers <= kMinimumVisualGpuLayerRetryCount) {
        return 0;
    }

    int32_t reduced =
        (current_layers * kVisualGpuLayerRetryScaleNumerator) /
        kVisualGpuLayerRetryScaleDenominator;
    if (reduced >= current_layers) {
        reduced = current_layers - 1;
    }
    return std::max(kMinimumVisualGpuLayerRetryCount, reduced);
}

std::vector<int32_t> build_visual_gpu_layer_retry_candidates(int32_t initial_layers) {
    std::vector<int32_t> candidates;
    append_unique_positive_visual_retry_layers(candidates, initial_layers);

    int32_t current_layers = initial_layers;
    while (current_layers > kMinimumVisualGpuLayerRetryCount) {
        current_layers = reduced_visual_retry_layers(current_layers);
        append_unique_positive_visual_retry_layers(candidates, current_layers);
    }

    return candidates;
}

int32_t estimate_visual_n_gpu_layers_with_headroom(const std::string& model_path,
                                                   const std::string& mmproj_path,
                                                   std::string_view backend_name,
                                                   size_t free_bytes,
                                                   size_t total_bytes);
llama_model_params cap_visual_model_gpu_layers_for_headroom(
    llama_model_params params,
    const std::string& model_path,
    const std::string& mmproj_path,
    const std::shared_ptr<spdlog::logger>& logger);

llama_model_params build_visual_model_params_for_path(
    const std::string& model_path,
    const std::string& mmproj_path,
    const std::shared_ptr<spdlog::logger>& logger) {
    const auto visual_override = resolve_visual_gpu_layer_override();
    const auto global_override = read_env_value("AI_FILE_SORTER_N_GPU_LAYERS");
    const auto legacy_override = read_env_value("LLAMA_CPP_N_GPU_LAYERS");

    std::optional<ScopedEnvValue> ai_override_guard;
    std::optional<ScopedEnvValue> legacy_override_guard;

    if (visual_override.has_value()) {
        const std::string override_value = std::to_string(*visual_override);
        ai_override_guard.emplace("AI_FILE_SORTER_N_GPU_LAYERS", override_value);
        legacy_override_guard.emplace("LLAMA_CPP_N_GPU_LAYERS", override_value);
        if (logger) {
            logger->info("Using visual-specific n_gpu_layers override={}", *visual_override);
        }
    } else {
        ai_override_guard.emplace("AI_FILE_SORTER_N_GPU_LAYERS", std::nullopt);
        legacy_override_guard.emplace("LLAMA_CPP_N_GPU_LAYERS", std::nullopt);
        if ((global_override.has_value() && !global_override->empty()) ||
            (legacy_override.has_value() && !legacy_override->empty())) {
            if (logger) {
                logger->info(
                    "Ignoring global n_gpu_layers override for visual analysis; "
                    "set AI_FILE_SORTER_VISUAL_N_GPU_LAYERS to override image analysis specifically.");
            }
        }
    }

    llama_model_params params = build_model_params_for_path(model_path, logger);
    if (!visual_override.has_value()) {
        params =
            cap_visual_model_gpu_layers_for_headroom(params, model_path, mmproj_path, logger);
    }
    return params;
}

bool is_mmproj_memory_guarded_backend(std::string_view backend_name) {
    return case_insensitive_contains(backend_name, "cuda") ||
           case_insensitive_contains(backend_name, "vulkan");
}

std::string guarded_backend_label(std::string_view backend_name) {
    if (case_insensitive_contains(backend_name, "cuda")) {
        return "CUDA";
    }
    if (case_insensitive_contains(backend_name, "vulkan")) {
        return "Vulkan";
    }
    return "GPU";
}

size_t required_mmproj_gpu_bytes(std::uintmax_t file_size) {
    constexpr size_t kMinHeadroomBytes = 512ULL * 1024ULL * 1024ULL;
    const size_t mmproj_bytes = static_cast<size_t>(
        std::min<std::uintmax_t>(file_size, std::numeric_limits<size_t>::max()));
    const size_t inflated_bytes = mmproj_bytes + (mmproj_bytes / 3);
    return inflated_bytes + kMinHeadroomBytes;
}

bool has_mmproj_gpu_headroom(size_t free_bytes, std::uintmax_t file_size) {
    return free_bytes >= required_mmproj_gpu_bytes(file_size);
}

#ifdef AI_FILE_SORTER_HAS_MTMD
std::optional<BackendMemoryInfo> query_cuda_backend_memory() {
    const auto memory = Utils::query_cuda_memory();
    if (!memory.has_value() || !memory->valid()) {
        return std::nullopt;
    }

    BackendMemoryInfo info;
    info.free_bytes = memory->free_bytes;
    info.total_bytes = (memory->total_bytes != 0) ? memory->total_bytes : memory->free_bytes;
    info.name = "CUDA";
    return info;
}

std::optional<BackendMemoryInfo> query_backend_memory(std::string_view backend_name) {
    if (case_insensitive_contains(backend_name, "cuda")) {
        return query_cuda_backend_memory();
    }

    const size_t device_count = ggml_backend_dev_count();
    BackendMemoryInfo best{};
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
        if (!backend_name.empty() && !case_insensitive_contains(reg_name ? reg_name : "", backend_name)) {
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

size_t saturating_add_bytes(size_t lhs, size_t rhs) {
    if (lhs > std::numeric_limits<size_t>::max() - rhs) {
        return std::numeric_limits<size_t>::max();
    }
    return lhs + rhs;
}

uint32_t read_visual_le32(const char* ptr) {
    uint32_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint32_t));
    return value;
}

uint64_t read_visual_le64(const char* ptr) {
    uint64_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint64_t));
    return value;
}

std::optional<int32_t> read_visual_uint_value(uint32_t type,
                                              const char* ptr,
                                              std::size_t available_bytes) {
    switch (type) {
        case 4: // GGUF_TYPE_UINT32
        case 5: // GGUF_TYPE_INT32
            if (available_bytes >= sizeof(uint32_t)) {
                return static_cast<int32_t>(read_visual_le32(ptr));
            }
            break;
        case 10: // GGUF_TYPE_UINT64
        case 11: // GGUF_TYPE_INT64
            if (available_bytes >= sizeof(uint64_t)) {
                return static_cast<int32_t>(read_visual_le64(ptr));
            }
            break;
        default:
            break;
    }
    return std::nullopt;
}

bool read_visual_file_prefix(std::ifstream& file,
                             std::vector<char>& buffer,
                             std::size_t requested_bytes,
                             std::size_t& bytes_read) {
    if (!file || requested_bytes == 0 || requested_bytes > buffer.size()) {
        return false;
    }

    file.read(buffer.data(), static_cast<std::streamsize>(requested_bytes));
    const std::streamsize count = file.gcount();
    if (count <= 0) {
        return false;
    }

    bytes_read = static_cast<std::size_t>(count);
    return true;
}

std::optional<int32_t> parse_visual_block_count_entry(const std::vector<char>& buffer,
                                                      std::size_t bytes_read,
                                                      std::size_t key_pos,
                                                      std::string_view key) {
    if (key_pos < sizeof(uint64_t)) {
        return std::nullopt;
    }

    const uint64_t declared_len =
        read_visual_le64(buffer.data() + key_pos - sizeof(uint64_t));
    if (declared_len != key.size()) {
        return std::nullopt;
    }

    const std::size_t type_offset = key_pos + key.size();
    if (type_offset + sizeof(uint32_t) > bytes_read) {
        return std::nullopt;
    }

    const uint32_t type = read_visual_le32(buffer.data() + type_offset);
    const std::size_t value_offset = type_offset + sizeof(uint32_t);
    if (value_offset >= bytes_read) {
        return std::nullopt;
    }

    return read_visual_uint_value(type,
                                  buffer.data() + value_offset,
                                  bytes_read - value_offset);
}

const std::array<const char*, 7>& visual_block_count_keys() {
    static const std::array<const char*, 7> keys = {
        "llama.block_count",
        "llama.layer_count",
        "llama.n_layer",
        "gemma.block_count",
        "qwen.block_count",
        "qwen2.block_count",
        "block_count",
    };
    return keys;
}

#ifdef AI_FILE_SORTER_HAS_MTMD
struct GgufCtxDeleter {
    void operator()(gguf_context* ctx) const {
        if (ctx) {
            gguf_free(ctx);
        }
    }
};

std::optional<int32_t> read_visual_gguf_numeric(gguf_context* ctx, int64_t id) {
    switch (gguf_get_kv_type(ctx, id)) {
        case GGUF_TYPE_INT16: return static_cast<int32_t>(gguf_get_val_i16(ctx, id));
        case GGUF_TYPE_INT32: return gguf_get_val_i32(ctx, id);
        case GGUF_TYPE_INT64: return static_cast<int32_t>(gguf_get_val_i64(ctx, id));
        case GGUF_TYPE_UINT16: return static_cast<int32_t>(gguf_get_val_u16(ctx, id));
        case GGUF_TYPE_UINT32: return static_cast<int32_t>(gguf_get_val_u32(ctx, id));
        case GGUF_TYPE_UINT64: return static_cast<int32_t>(gguf_get_val_u64(ctx, id));
        default:
            return std::nullopt;
    }
}

std::optional<int32_t> try_visual_block_count_keys(gguf_context* ctx) {
    for (const char* key : visual_block_count_keys()) {
        const int64_t id = gguf_find_key(ctx, key);
        if (id < 0) {
            continue;
        }
        if (auto value = read_visual_gguf_numeric(ctx, id)) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<int32_t> infer_visual_block_count_from_tensors(gguf_context* ctx) {
    const int64_t tensor_count = gguf_get_n_tensors(ctx);
    int32_t max_layer = -1;

    for (int64_t i = 0; i < tensor_count; ++i) {
        const char* tensor_name = gguf_get_tensor_name(ctx, i);
        if (!tensor_name) {
            continue;
        }

        int32_t current = -1;
        for (const char* p = tensor_name; *p; ++p) {
            if (!std::isdigit(static_cast<unsigned char>(*p))) {
                continue;
            }

            int value = 0;
            while (*p && std::isdigit(static_cast<unsigned char>(*p))) {
                value = value * 10 + (*p - '0');
                ++p;
            }
            current = std::max(current, value);
        }

        if (current > max_layer) {
            max_layer = current;
        }
    }

    if (max_layer >= 0) {
        return max_layer + 1;
    }
    return std::nullopt;
}

bool has_gguf_magic(const std::string& model_path) {
    std::ifstream file(model_path, std::ios::binary);
    if (!file) {
        return false;
    }

    char magic[4] = {};
    file.read(magic, sizeof(magic));
    return file.gcount() == static_cast<std::streamsize>(sizeof(magic)) &&
           std::memcmp(magic, "GGUF", sizeof(magic)) == 0;
}

std::optional<int32_t> extract_visual_block_count_gguf(const std::string& model_path) {
    if (!has_gguf_magic(model_path)) {
        return std::nullopt;
    }

    gguf_init_params params{};
    params.no_alloc = true;
    gguf_context* ctx = gguf_init_from_file(model_path.c_str(), params);
    if (!ctx) {
        return std::nullopt;
    }

    auto cleanup = std::unique_ptr<gguf_context, GgufCtxDeleter>(ctx);
    if (auto block_count = try_visual_block_count_keys(ctx)) {
        return block_count;
    }
    return infer_visual_block_count_from_tensors(ctx);
}
#endif

std::optional<int32_t> extract_visual_block_count_from_scan(const std::string& model_path) {
    std::ifstream file(model_path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::vector<char> buffer(kVisualMetadataScanBytes);
    std::streamsize to_read = static_cast<std::streamsize>(buffer.size());
    std::error_code size_ec;
    const auto file_size = std::filesystem::file_size(model_path, size_ec);
    if (!size_ec) {
        to_read = static_cast<std::streamsize>(std::min<std::uintmax_t>(file_size, buffer.size()));
    }

    if (to_read <= 0 || static_cast<std::size_t>(to_read) > buffer.size()) {
        return std::nullopt;
    }

    std::size_t bytes_read = 0;
    if (!read_visual_file_prefix(file, buffer, static_cast<std::size_t>(to_read), bytes_read)) {
        return std::nullopt;
    }

    const std::string_view data(buffer.data(), bytes_read);
    for (const char* key : visual_block_count_keys()) {
        std::size_t pos = data.find(key);
        while (pos != std::string_view::npos) {
            if (const auto parsed =
                    parse_visual_block_count_entry(buffer, bytes_read, pos, key)) {
                return parsed;
            }
            pos = data.find(key, pos + 1);
        }
    }

    return std::nullopt;
}

std::optional<int32_t> extract_visual_block_count(const std::string& model_path) {
#ifdef AI_FILE_SORTER_HAS_MTMD
    if (const auto block_count = extract_visual_block_count_gguf(model_path)) {
        return block_count;
    }
#endif
    return extract_visual_block_count_from_scan(model_path);
}

struct VisualLayerMetrics {
    int32_t total_layers = 0;
    double bytes_per_layer = 0.0;
};

bool populate_visual_layer_metrics(const std::string& model_path, VisualLayerMetrics& metrics) {
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(model_path, ec);
    if (ec) {
        return false;
    }

    const auto block_count = extract_visual_block_count(model_path);
    if (!block_count.has_value() || block_count.value() <= 0) {
        return false;
    }

    metrics.total_layers = *block_count;
    metrics.bytes_per_layer =
        static_cast<double>(file_size) / static_cast<double>(metrics.total_layers);
    return metrics.bytes_per_layer > 0.0;
}

Utils::CudaMemoryInfo subtract_visual_headroom(size_t free_bytes,
                                               size_t total_bytes,
                                               size_t reserve_bytes) {
    Utils::CudaMemoryInfo adjusted;
    if (free_bytes <= reserve_bytes) {
        return adjusted;
    }

    adjusted.free_bytes = free_bytes - reserve_bytes;
    const size_t usable_total = (total_bytes != 0) ? total_bytes : free_bytes;
    adjusted.total_bytes =
        (usable_total > reserve_bytes) ? usable_total - reserve_bytes : adjusted.free_bytes;
    return adjusted;
}

int32_t estimate_visual_n_gpu_layers_from_adjusted_memory(const VisualLayerMetrics& metrics,
                                                          const Utils::CudaMemoryInfo& memory) {
    if (metrics.total_layers <= 0 || metrics.bytes_per_layer <= 0.0 || !memory.valid()) {
        return 0;
    }

    double approx_free = static_cast<double>(memory.free_bytes);
    double total = static_cast<double>(memory.total_bytes);
    if (total <= 0.0) {
        total = approx_free;
    }

    const double usable_total = std::max(total, approx_free);
    if (usable_total <= 0.0 || approx_free <= 0.0) {
        return 0;
    }

    const double safety_reserve =
        std::max(usable_total * kVisualSafetyReserveFraction,
                 kVisualMinimumSafetyReserveBytes);
    double budget = approx_free - safety_reserve;
    if (budget <= 0.0) {
        budget = approx_free * kVisualBudgetFallbackFraction;
    }

    const double max_budget =
        std::min(approx_free * kVisualMaxApproxFreeBudgetFraction,
                 usable_total * kVisualMaxUsableTotalBudgetFraction);
    if (max_budget <= 0.0) {
        return 0;
    }

    const double min_budget =
        std::min(approx_free * kVisualMinApproxFreeBudgetFraction, max_budget);
    budget = std::clamp(budget, min_budget, max_budget);

    const double denominator = metrics.bytes_per_layer * kVisualLayerOverheadFactor;
    if (denominator <= 0.0) {
        return 0;
    }

    const int32_t estimated_layers =
        static_cast<int32_t>(std::floor(budget / denominator));
    if (estimated_layers <= 0) {
        return 0;
    }

    return std::clamp<int32_t>(estimated_layers, 1, metrics.total_layers);
}

std::optional<std::string> resolve_visual_headroom_backend_name() {
    if (const auto backend = read_env_value("AI_FILE_SORTER_GPU_BACKEND")) {
        const std::string lowered = to_lower_copy(*backend);
        if (lowered == "cpu") {
            return std::nullopt;
        }
        if (lowered == "cuda" || lowered == "vulkan") {
            return lowered;
        }
    }

    if (const auto device = read_env_value("LLAMA_ARG_DEVICE")) {
        const std::string lowered = to_lower_copy(*device);
        if (lowered == "cpu") {
            return std::nullopt;
        }
        if (lowered == "cuda" || lowered == "vulkan") {
            return lowered;
        }
    }

    if (read_env_bool("GGML_DISABLE_CUDA").value_or(false)) {
        if (query_backend_memory("vulkan").has_value()) {
            return std::string("vulkan");
        }
        return std::nullopt;
    }

    if (Utils::is_cuda_available()) {
        return std::string("cuda");
    }
    if (query_backend_memory("vulkan").has_value()) {
        return std::string("vulkan");
    }
    return std::nullopt;
}

int32_t estimate_visual_n_gpu_layers_with_headroom(const std::string& model_path,
                                                   const std::string& mmproj_path,
                                                   std::string_view backend_name,
                                                   size_t free_bytes,
                                                   size_t total_bytes) {
    if (!is_mmproj_memory_guarded_backend(backend_name)) {
        return -1;
    }

    std::error_code mmproj_ec;
    const auto mmproj_size = std::filesystem::file_size(mmproj_path, mmproj_ec);
    if (mmproj_ec) {
        return -1;
    }

    const size_t reserve_bytes =
        saturating_add_bytes(required_mmproj_gpu_bytes(mmproj_size), kVisualEvalHeadroomBytes);
    const Utils::CudaMemoryInfo adjusted_memory =
        subtract_visual_headroom(free_bytes, total_bytes, reserve_bytes);
    const size_t raw_total_bytes = (total_bytes != 0) ? total_bytes : free_bytes;
    Utils::CudaMemoryInfo total_vram_memory;
    total_vram_memory.free_bytes = raw_total_bytes;
    total_vram_memory.total_bytes = raw_total_bytes;
    const int32_t total_vram_heuristic_layers =
        Utils::compute_ngl_from_cuda_memory(total_vram_memory);

    VisualLayerMetrics metrics;
    const int heuristic_layers = Utils::compute_ngl_from_cuda_memory(adjusted_memory);
    if (!populate_visual_layer_metrics(model_path, metrics)) {
        return std::max(heuristic_layers, total_vram_heuristic_layers);
    }

    int32_t candidate =
        estimate_visual_n_gpu_layers_from_adjusted_memory(metrics, adjusted_memory);
    if (candidate <= 0) {
        return heuristic_layers;
    }
    if (heuristic_layers > 0) {
        candidate = std::max(candidate, heuristic_layers);
    }
    if (total_vram_heuristic_layers > candidate &&
        total_vram_heuristic_layers - candidate <=
            kVisualTierHeuristicReconciliationWindowLayers) {
        candidate = total_vram_heuristic_layers;
    }
    return candidate;
}

llama_model_params cap_visual_model_gpu_layers_for_headroom(
    llama_model_params params,
    const std::string& model_path,
    const std::string& mmproj_path,
    const std::shared_ptr<spdlog::logger>& logger) {
    if (params.n_gpu_layers <= 0) {
        return params;
    }

    const auto backend_name = resolve_visual_headroom_backend_name();
    if (!backend_name.has_value()) {
        return params;
    }

    const auto memory = query_backend_memory(*backend_name);
    if (!memory.has_value()) {
        return params;
    }

    const int32_t capped_layers = estimate_visual_n_gpu_layers_with_headroom(
        model_path,
        mmproj_path,
        *backend_name,
        memory->free_bytes,
        memory->total_bytes);
    if (capped_layers < 0 || capped_layers >= params.n_gpu_layers) {
        return params;
    }

    if (logger) {
        logger->info(
            "Capping visual n_gpu_layers from {} to {} on {} to preserve mmproj and multimodal eval headroom.",
            params.n_gpu_layers,
            capped_layers,
            guarded_backend_label(*backend_name));
    }

    params.n_gpu_layers = capped_layers;
    return params;
}

bool should_enable_mmproj_gpu(const std::filesystem::path& mmproj_path,
                              std::string_view backend_name,
                              const std::shared_ptr<spdlog::logger>& logger) {
    if (!is_mmproj_memory_guarded_backend(backend_name)) {
        return true;
    }

    const auto backend_label = guarded_backend_label(backend_name);
    const auto memory = query_backend_memory(backend_name);
    if (!memory.has_value()) {
        if (logger) {
            logger->warn("{} memory metrics unavailable; using CPU for visual encoder to avoid OOM. "
                         "Set AI_FILE_SORTER_VISUAL_USE_GPU=1 to force GPU.",
                         backend_label);
        }
        return false;
    }

    std::error_code ec;
    const auto file_size = std::filesystem::file_size(mmproj_path, ec);
    if (ec) {
        if (logger) {
            logger->warn("Failed to stat mmproj file '{}'; using CPU for visual encoder to avoid OOM. "
                         "Set AI_FILE_SORTER_VISUAL_USE_GPU=1 to force GPU.",
                         Utils::path_to_utf8(mmproj_path));
        }
        return false;
    }

    const size_t required_bytes = required_mmproj_gpu_bytes(file_size);

    if (!has_mmproj_gpu_headroom(memory->free_bytes, file_size)) {
        if (logger) {
            logger->warn(
                "{} free memory {:.1f} MiB < {:.1f} MiB needed for mmproj; using CPU for visual encoder. "
                "Set AI_FILE_SORTER_VISUAL_USE_GPU=1 to force GPU.",
                backend_label,
                memory->free_bytes / kBytesPerMiB,
                required_bytes / kBytesPerMiB);
        }
        return false;
    }

    return true;
}

struct BitmapDeleter {
    void operator()(mtmd_bitmap* ptr) const {
        if (ptr) {
            mtmd_bitmap_free(ptr);
        }
    }
};

struct ChunkDeleter {
    void operator()(mtmd_input_chunks* ptr) const {
        if (ptr) {
            mtmd_input_chunks_free(ptr);
        }
    }
};

using BitmapPtr = std::unique_ptr<mtmd_bitmap, BitmapDeleter>;
using ChunkPtr = std::unique_ptr<mtmd_input_chunks, ChunkDeleter>;

llama_token greedy_sample(const float* logits, int vocab_size, float temperature) {
    const float temp = std::max(temperature, 1e-3f);
    int best_index = 0;
    float best_value = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < vocab_size; ++i) {
        const float value = logits[i] / temp;
        if (value > best_value) {
            best_value = value;
            best_index = i;
        }
    }
    return static_cast<llama_token>(best_index);
}

bool format_visual_prompt(llama_model* model,
                          std::string_view system_prompt,
                          std::string_view user_prompt,
                          std::string& final_prompt) {
    if (!model) {
        return false;
    }

    const char* tmpl = llama_model_chat_template(model, nullptr);
    if (!tmpl || tmpl[0] == '\0') {
        final_prompt.clear();
        if (!system_prompt.empty()) {
            final_prompt.append(system_prompt);
            final_prompt.append("\n\n");
        }
        final_prompt.append(user_prompt);
        return true;
    }

    std::vector<std::string> owned_messages;
    owned_messages.reserve(system_prompt.empty() ? 1 : 2);
    std::vector<llama_chat_message> messages;
    messages.reserve(system_prompt.empty() ? 1 : 2);

    if (!system_prompt.empty()) {
        owned_messages.emplace_back(system_prompt);
        messages.push_back({"system", owned_messages.back().c_str()});
    }

    owned_messages.emplace_back(user_prompt);
    messages.push_back({"user", owned_messages.back().c_str()});

    std::size_t estimated_size = kEstimatedPromptBufferBytes;
    for (const auto& message : owned_messages) {
        estimated_size += message.size() * 2;
    }

    std::vector<char> formatted_prompt(estimated_size);
    int actual_len = llama_chat_apply_template(tmpl,
                                               messages.data(),
                                               messages.size(),
                                               true,
                                               formatted_prompt.data(),
                                               static_cast<int32_t>(formatted_prompt.size()));
    if (actual_len < 0) {
        return false;
    }

    if (actual_len >= static_cast<int>(formatted_prompt.size())) {
        formatted_prompt.resize(static_cast<std::size_t>(actual_len) + 1);
        actual_len = llama_chat_apply_template(tmpl,
                                               messages.data(),
                                               messages.size(),
                                               true,
                                               formatted_prompt.data(),
                                               static_cast<int32_t>(formatted_prompt.size()));
        if (actual_len < 0 || actual_len >= static_cast<int>(formatted_prompt.size())) {
            return false;
        }
    }

    final_prompt.assign(formatted_prompt.data(), static_cast<std::size_t>(actual_len));
    return true;
}

#else

int32_t estimate_visual_n_gpu_layers_with_headroom(const std::string&,
                                                   const std::string&,
                                                   std::string_view,
                                                   size_t,
                                                   size_t) {
    return -1;
}

llama_model_params cap_visual_model_gpu_layers_for_headroom(
    llama_model_params params,
    const std::string&,
    const std::string&,
    const std::shared_ptr<spdlog::logger>&) {
    return params;
}

#endif

} // namespace

#ifdef AI_FILE_SORTER_TEST_BUILD
namespace LlavaImageAnalyzerTestAccess {
int32_t default_visual_batch_size(bool gpu_enabled, std::string_view backend_name) {
    return resolve_default_visual_batch_size(gpu_enabled, backend_name);
}

int32_t visual_model_n_gpu_layers_for_model(const std::string& model_path) {
    return build_visual_model_params_for_path(model_path, model_path, nullptr).n_gpu_layers;
}

std::vector<int32_t> visual_gpu_layer_retry_candidates(int32_t initial_layers) {
    return build_visual_gpu_layer_retry_candidates(initial_layers);
}

int32_t visual_model_n_gpu_layers_with_headroom(const std::string& model_path,
                                                const std::string& mmproj_path,
                                                std::string_view backend_name,
                                                size_t free_bytes,
                                                size_t total_bytes) {
    return estimate_visual_n_gpu_layers_with_headroom(
        model_path, mmproj_path, backend_name, free_bytes, total_bytes);
}

bool should_use_mmproj_gpu_for_memory(std::string_view backend_name,
                                      size_t free_bytes,
                                      std::uintmax_t mmproj_file_size) {
    return !is_mmproj_memory_guarded_backend(backend_name) ||
           has_mmproj_gpu_headroom(free_bytes, mmproj_file_size);
}

std::string description_system_prompt(VisualPromptPolicy policy) {
    return build_description_request(policy).system_prompt;
}

std::string description_user_prompt(VisualPromptPolicy policy) {
    return build_description_request(policy).user_prompt;
}

std::string filename_system_prompt(VisualPromptPolicy policy) {
    return build_filename_request(policy, "example description").system_prompt;
}

std::string filename_user_prompt(VisualPromptPolicy policy, std::string_view description) {
    return build_filename_request(policy, description).user_prompt;
}
}
#endif

LlavaImageAnalyzer::LlavaImageAnalyzer(const std::filesystem::path& model_path,
                                       const std::filesystem::path& mmproj_path,
                                       VisualPromptPolicy prompt_policy)
    : LlavaImageAnalyzer(model_path, mmproj_path, prompt_policy, Settings{}) {}

LlavaImageAnalyzer::LlavaImageAnalyzer(const std::filesystem::path& model_path,
                                       const std::filesystem::path& mmproj_path,
                                       VisualPromptPolicy prompt_policy,
                                       Settings settings)
#ifdef AI_FILE_SORTER_HAS_MTMD
    : model_path_(model_path)
    , mmproj_path_(mmproj_path)
    , settings_(settings)
    , prompt_policy_(prompt_policy)
#else
    : settings_(settings)
    , prompt_policy_(prompt_policy)
#endif
{
    if (settings_.n_threads <= 0) {
        settings_.n_threads = static_cast<int32_t>(std::max(1u, std::thread::hardware_concurrency()));
    }

#ifndef AI_FILE_SORTER_HAS_MTMD
    (void)model_path;
    (void)mmproj_path;
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->error("Visual LLM support is not available in this build.");
    }
    return;
#else
    visual_gpu_override_ = read_env_bool("AI_FILE_SORTER_VISUAL_USE_GPU");
    if (visual_gpu_override_.has_value()) {
        settings_.use_gpu = *visual_gpu_override_;
    }
    const bool has_explicit_visual_gpu_layer_override =
        resolve_visual_gpu_layer_override().has_value();

    auto logger = Logger::get_logger("core_logger");
    const std::string backend_name = resolve_backend_name();
    const auto cleanup = [this]() {
        if (vision_ctx_) {
            mtmd_free(vision_ctx_);
            vision_ctx_ = nullptr;
        }
        if (context_) {
            llama_free(context_);
            context_ = nullptr;
        }
        if (model_) {
            llama_model_free(model_);
            model_ = nullptr;
        }
    };

    llama_model_params model_params = llama_model_default_params();
    const std::string model_path_utf8 = Utils::path_to_utf8(model_path);
    const std::string mmproj_path_utf8 = Utils::path_to_utf8(mmproj_path);
    if (settings_.use_gpu) {
        model_params =
            build_visual_model_params_for_path(model_path_utf8, mmproj_path_utf8, logger);
    } else {
        model_params.n_gpu_layers = 0;
    }

    auto try_load_visual_model = [&](const llama_model_params& params) -> bool {
        model_ = llama_model_load_from_file(model_path_utf8.c_str(), params);
        return model_ != nullptr;
    };

    if (!try_load_visual_model(model_params)) {
        if (model_params.n_gpu_layers > 0 && !has_explicit_visual_gpu_layer_override) {
            const auto retry_layers =
                build_visual_gpu_layer_retry_candidates(model_params.n_gpu_layers);
            for (std::size_t i = 1; i < retry_layers.size(); ++i) {
                const int32_t previous_layers = retry_layers[i - 1];
                const int32_t retry_layers_count = retry_layers[i];
                if (logger) {
                    logger->warn(
                        "Failed to load visual text model with n_gpu_layers={}; retrying with n_gpu_layers={}.",
                        previous_layers,
                        retry_layers_count);
                }

                llama_model_params retry_params = model_params;
                retry_params.n_gpu_layers = retry_layers_count;
                if (try_load_visual_model(retry_params)) {
                    if (logger) {
                        logger->info(
                            "Loaded visual text model '{}' after reducing n_gpu_layers from {} to {}.",
                            model_path_utf8,
                            model_params.n_gpu_layers,
                            retry_layers_count);
                    }
                    model_params = retry_params;
                    break;
                }
            }
        }
    }
    if (!model_) {
        throw std::runtime_error("Failed to load visual text model at " + model_path_utf8);
    }

    text_gpu_enabled_ = settings_.use_gpu && model_params.n_gpu_layers != 0;
    context_tokens_ = settings_.n_ctx;
    batch_size_ = resolve_default_visual_batch_size(text_gpu_enabled_, backend_name);

    vocab_ = llama_model_get_vocab(model_);

    bool mmproj_use_gpu = text_gpu_enabled_;
    if (mmproj_use_gpu && (!visual_gpu_override_.has_value() || !*visual_gpu_override_)) {
        mmproj_use_gpu = should_enable_mmproj_gpu(mmproj_path, backend_name, logger);
    }
    const bool allow_mmproj_cpu_retry =
        mmproj_use_gpu && (!visual_gpu_override_.has_value() || !*visual_gpu_override_);

    auto try_init_mmproj = [&](bool use_gpu) -> bool {
        if (vision_ctx_) {
            mtmd_free(vision_ctx_);
            vision_ctx_ = nullptr;
        }

        mtmd_context_params mm_params = mtmd_context_params_default();
        mm_params.use_gpu = use_gpu;
        mm_params.n_threads = settings_.n_threads;
        vision_ctx_ = mtmd_init_from_file(mmproj_path_utf8.c_str(), model_, mm_params);
        if (!vision_ctx_) {
            return false;
        }
        if (!mtmd_support_vision(vision_ctx_)) {
            mtmd_free(vision_ctx_);
            vision_ctx_ = nullptr;
            throw std::runtime_error("The provided multimodal projector does not expose vision capabilities");
        }
        mmproj_gpu_enabled_ = use_gpu;
        return true;
    };

    try {
        if (!try_init_mmproj(mmproj_use_gpu)) {
            if (allow_mmproj_cpu_retry) {
                if (logger) {
                    logger->warn("Failed to load multimodal projector on GPU; retrying with CPU projector execution.");
                }
                if (!try_init_mmproj(false)) {
                    cleanup();
                    throw std::runtime_error("Failed to load multimodal projector file at " + mmproj_path_utf8);
                }
            } else {
                cleanup();
                throw std::runtime_error("Failed to load multimodal projector file at " + mmproj_path_utf8);
            }
        }
    } catch (...) {
        cleanup();
        throw;
    }
    try {
        initialize_context();
    } catch (...) {
        cleanup();
        throw;
    }
#endif
}

LlavaImageAnalyzer::~LlavaImageAnalyzer() {
#ifdef AI_FILE_SORTER_HAS_MTMD
    if (vision_ctx_) {
        mtmd_free(vision_ctx_);
        vision_ctx_ = nullptr;
    }
    if (context_) {
        llama_free(context_);
        context_ = nullptr;
    }
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
#endif
}

#ifdef AI_FILE_SORTER_HAS_MTMD
void LlavaImageAnalyzer::initialize_context() {
    auto logger = Logger::get_logger("core_logger");
    const int32_t initial_ctx = context_tokens_ > 0 ? context_tokens_ : settings_.n_ctx;
    const int32_t initial_batch =
        batch_size_ > 0 ? batch_size_ : kDefaultVisualCpuBatchSize;

    auto try_init_context = [&](int32_t n_ctx, int32_t n_batch) -> bool {
        if (context_) {
            llama_free(context_);
            context_ = nullptr;
        }

        const int32_t bounded_ctx = std::max<int32_t>(1, n_ctx);
        const int32_t bounded_batch = std::max<int32_t>(1, std::min(n_batch, bounded_ctx));

        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = bounded_ctx;
        ctx_params.n_batch = bounded_batch;
        ctx_params.n_ubatch = bounded_batch;
        ctx_params.n_threads = settings_.n_threads;
        ctx_params.n_threads_batch = settings_.n_threads;
        context_ = llama_init_from_model(model_, ctx_params);
        if (context_) {
            llama_set_n_threads(context_, settings_.n_threads, settings_.n_threads);
        }
        return context_ != nullptr;
    };

    auto apply_context_limits = [&](int32_t n_ctx, int32_t n_batch) {
        context_tokens_ = std::max<int32_t>(1, n_ctx);
        batch_size_ = std::max<int32_t>(1, std::min(n_batch, context_tokens_));
    };

    bool context_ready = try_init_context(initial_ctx, initial_batch);
    if (context_ready) {
        apply_context_limits(initial_ctx, initial_batch);
    } else if (initial_batch > kDefaultVisualGpuBatchSize) {
        if (logger) {
            logger->warn("Failed to initialize llama_context (n_ctx={}, n_batch={}); retrying with smaller batch.",
                         initial_ctx, initial_batch);
        }
        const int32_t smaller_batch = kDefaultVisualGpuBatchSize;
        context_ready = try_init_context(initial_ctx, smaller_batch);
        if (context_ready) {
            apply_context_limits(initial_ctx, smaller_batch);
        }
    }
    if (!context_ready) {
        if (logger) {
            logger->warn("Failed to initialize llama_context (n_ctx={}, n_batch={}); retrying with smaller buffers.",
                         initial_ctx, std::min(initial_batch, kDefaultVisualGpuBatchSize));
        }
        const int32_t reduced_ctx = std::min(initial_ctx, kVisualReducedContextTokens);
        const int32_t reduced_batch =
            std::min(initial_batch, kDefaultVisualGpuBatchSize);
        context_ready = try_init_context(reduced_ctx, reduced_batch);
        if (context_ready) {
            apply_context_limits(reduced_ctx, reduced_batch);
        } else {
            const int32_t smaller_batch =
                std::min(reduced_batch, kVisualBatchRetrySize);
            context_ready = try_init_context(reduced_ctx, smaller_batch);
            if (context_ready) {
                apply_context_limits(reduced_ctx, smaller_batch);
            } else {
                const int32_t smaller_ctx =
                    std::min(reduced_ctx, kVisualMinimumContextTokens);
                context_ready = try_init_context(smaller_ctx, smaller_batch);
                if (context_ready) {
                    apply_context_limits(smaller_ctx, smaller_batch);
                }
            }
        }
    }

    if (!context_ready) {
        std::string hint;
        if (text_gpu_enabled_) {
            hint = " (try AI_FILE_SORTER_VISUAL_USE_GPU=0 to force CPU)";
        }
        throw std::runtime_error("Failed to create llama_context" + hint);
    }

    reset_context_state();
}

void LlavaImageAnalyzer::reset_context_state() {
    if (!context_) {
        return;
    }
    llama_memory_clear(llama_get_memory(context_), true);
    llama_synchronize(context_);
    llama_perf_context_reset(context_);
}
#endif

bool LlavaImageAnalyzer::is_supported_image(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return false;
    }
    std::string ext = to_lower_copy(Utils::path_to_utf8(path.extension()));
    static const std::unordered_set<std::string> kExtensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tga", ".psd", ".hdr",
        ".pic", ".pnm", ".ppm", ".pgm", ".pbm"
    };
    return kExtensions.find(ext) != kExtensions.end();
}

ImageAnalysisResult LlavaImageAnalyzer::analyze(const std::filesystem::path& image_path) {
#ifndef AI_FILE_SORTER_HAS_MTMD
    (void)image_path;
    throw std::runtime_error("Visual LLM support is not available in this build.");
#else
    auto logger = Logger::get_logger("core_logger");
    const auto analysis_started = std::chrono::steady_clock::now();
    const std::string image_path_utf8 = Utils::path_to_utf8(image_path);
    const auto bitmap_started = std::chrono::steady_clock::now();
    const auto loaded_bitmap =
        mtmd_helper_bitmap_init_from_file(vision_ctx_, image_path_utf8.c_str(), false);
    BitmapPtr bitmap(loaded_bitmap.bitmap);
    if (!bitmap) {
        throw std::runtime_error("Failed to load image for visual analysis: " + image_path_utf8);
    }
    ImageAnalysisDiagnostics diagnostics;
    diagnostics.available = true;
    diagnostics.bitmap_load_ms = elapsed_ms(bitmap_started, std::chrono::steady_clock::now());
    diagnostics.batch_size = batch_size_;
    diagnostics.text_gpu_enabled = text_gpu_enabled_;
    diagnostics.mmproj_gpu_enabled = mmproj_gpu_enabled_;

    const auto description_request = build_description_request(prompt_policy_);
    const std::string description = infer_text(bitmap.get(),
                                               description_request.system_prompt,
                                               description_request.user_prompt,
                                               settings_.n_predict,
                                               &diagnostics.description_pass);

    const auto filename_request = build_filename_request(prompt_policy_, description);
    const std::string raw_filename = infer_text(nullptr,
                                                filename_request.system_prompt,
                                                filename_request.user_prompt,
                                                settings_.n_predict,
                                                &diagnostics.filename_pass);
    diagnostics.batch_size = batch_size_;
    if (logger) {
        logger->info("Visual raw filename: {}", raw_filename);
    }
    std::string filename_base = sanitize_filename(raw_filename, kMaxFilenameWords, kMaxFilenameLength);
    if (filename_base.empty()) {
        filename_base = sanitize_filename(description, kMaxFilenameWords, kMaxFilenameLength);
    }
    if (filename_base.empty()) {
        filename_base = "image_" + slugify(Utils::path_to_utf8(image_path.stem()));
    }

    ImageAnalysisResult result;
    result.description = description;
    result.suggested_name = normalize_filename(filename_base, image_path);
    diagnostics.total_ms = elapsed_ms(analysis_started, std::chrono::steady_clock::now());
    result.diagnostics = diagnostics;
    if (logger) {
        logger->info("Visual suggested filename: {}", result.suggested_name);
    }
    return result;
#endif
}

#ifdef AI_FILE_SORTER_HAS_MTMD
void LlavaImageAnalyzer::mtmd_progress_callback(const char* name,
                                                int32_t current_batch,
                                                int32_t total_batches,
                                                void* user_data) {
    if (!user_data || total_batches <= 0 || current_batch <= 0) {
        return;
    }
    if (name && std::strcmp(name, "image") != 0) {
        return;
    }
    auto* self = static_cast<LlavaImageAnalyzer*>(user_data);
    if (!self->settings_.batch_progress) {
        return;
    }
    self->image_batch_current_.store(current_batch, std::memory_order_relaxed);
    self->image_batch_total_.store(total_batches, std::memory_order_relaxed);
    self->settings_.batch_progress(current_batch, total_batches);
}

#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
void LlavaImageAnalyzer::mtmd_log_callback(enum ggml_log_level level,
                                           const char* text,
                                           void* user_data) {
    (void)level;
    if (!text) {
        return;
    }
    auto* self = static_cast<LlavaImageAnalyzer*>(user_data);
#if !defined(AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
    if (self && self->settings_.batch_progress) {
        int current_batch = 0;
        int total_batches = 0;
        if (std::sscanf(text, "decoding image batch %d/%d",
                        &current_batch,
                        &total_batches) == 2) {
            if (current_batch > 0 && total_batches > 0) {
                self->settings_.batch_progress(current_batch, total_batches);
            }
        }
    }
#endif
    if (!self || !self->settings_.log_visual_output) {
        return;
    }
    if (is_mtmd_prompt_log_line(text)) {
        return;
    }
    std::fputs(text, stderr);
    std::fflush(stderr);
}
#endif

#endif

#ifdef AI_FILE_SORTER_HAS_MTMD
std::string LlavaImageAnalyzer::infer_text(mtmd_bitmap* bitmap,
                                           std::string_view system_prompt,
                                           const std::string& user_prompt,
                                           int32_t max_tokens,
                                           ImageInferenceDiagnostics* diagnostics) {
    auto logger = Logger::get_logger("core_logger");
    if (!context_) {
        initialize_context();
    }
    reset_context_state();

    if (diagnostics) {
        *diagnostics = {};
        diagnostics->used_image = bitmap != nullptr;
    }
    if (bitmap) {
        image_batch_current_.store(0, std::memory_order_relaxed);
        image_batch_total_.store(0, std::memory_order_relaxed);
    }

    const auto inference_started = std::chrono::steady_clock::now();

    std::string formatted_prompt;
    if (!format_visual_prompt(model_, system_prompt, user_prompt, formatted_prompt) || formatted_prompt.empty()) {
        formatted_prompt = user_prompt;
    }

    ChunkPtr chunks(mtmd_input_chunks_init());
    if (!chunks) {
        throw std::runtime_error("Failed to allocate mtmd input chunks");
    }

    mtmd_input_text text{};
    text.text = formatted_prompt.c_str();
    text.add_special = true;
    text.parse_special = true;

    const mtmd_bitmap* bitmaps[] = { bitmap };
    const mtmd_bitmap** bitmap_ptr = nullptr;
    int32_t bitmap_count = 0;
    if (bitmap) {
        bitmap_ptr = bitmaps;
        bitmap_count = 1;
    }

#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
    struct LogGuard {
        bool active{false};
        explicit LogGuard(LlavaImageAnalyzer* self)
            : active(set_mtmd_log_callback(&LlavaImageAnalyzer::mtmd_log_callback, self)) {
        }
        ~LogGuard() {
            if (!active) {
                return;
            }
            set_mtmd_log_callback(nullptr, nullptr);
        }
    };

    LogGuard log_guard(this);
#endif

    const auto tokenize_started = std::chrono::steady_clock::now();
    const int32_t tokenize_res = mtmd_tokenize(
        vision_ctx_,
        chunks.get(),
        &text,
        bitmap_ptr,
        bitmap_count);
    const auto tokenize_finished = std::chrono::steady_clock::now();
    if (diagnostics) {
        diagnostics->tokenize_ms = elapsed_ms(tokenize_started, tokenize_finished);
    }
    if (tokenize_res != 0) {
        throw std::runtime_error("mtmd_tokenize failed with code " + std::to_string(tokenize_res));
    }

#if defined(AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
    struct ProgressGuard {
        bool active{false};
        ProgressGuard(bool enabled, LlavaImageAnalyzer* self)
            : active(enabled &&
                     set_mtmd_progress_callback(&LlavaImageAnalyzer::mtmd_progress_callback, self)) {
        }
        ~ProgressGuard() {
            if (!active) {
                return;
            }
            set_mtmd_progress_callback(nullptr, nullptr);
        }
    };

    const bool enable_progress = bitmap && settings_.batch_progress;
    ProgressGuard progress_guard(enable_progress, this);
#endif

    llama_pos new_n_past = 0;
    const auto eval_started = std::chrono::steady_clock::now();
    auto eval_chunks = [&](int32_t n_batch) {
        return mtmd_helper_eval_chunks(vision_ctx_,
                                       context_,
                                       chunks.get(),
                                       0 /* n_past */,
                                       0 /* seq_id */,
                                       n_batch,
                                       true /* logits_last */,
                                       &new_n_past);
    };

    int32_t eval_batch =
        (batch_size_ > 0 ? batch_size_ : kDefaultVisualCpuBatchSize);
    int eval_result = eval_chunks(eval_batch);
    if (eval_result != 0 && bitmap && text_gpu_enabled_) {
        for (const int32_t retry_batch : visual_eval_retry_batches(eval_batch)) {
            if (logger) {
                logger->warn(
                    "mtmd_helper_eval_chunks failed with n_batch={}; retrying image eval with n_batch={}.",
                    eval_batch,
                    retry_batch);
            }
            batch_size_ = retry_batch;
            initialize_context();
            eval_batch = batch_size_;
            eval_result = eval_chunks(eval_batch);
            if (eval_result == 0) {
                break;
            }
        }
    }
    if (eval_result != 0) {
        throw std::runtime_error("mtmd_helper_eval_chunks failed");
    }
    const auto eval_finished = std::chrono::steady_clock::now();
    if (diagnostics) {
        diagnostics->eval_ms = elapsed_ms(eval_started, eval_finished);
        if (bitmap) {
            diagnostics->image_batch_current =
                image_batch_current_.load(std::memory_order_relaxed);
            diagnostics->image_batch_total =
                image_batch_total_.load(std::memory_order_relaxed);
        }
    }

    std::string response;
    response.reserve(kGeneratedResponseReserveBytes);

    const int vocab_size = llama_vocab_n_tokens(vocab_);
    const auto generation_started = std::chrono::steady_clock::now();
    for (int32_t i = 0; i < max_tokens; ++i) {
        const float* logits = llama_get_logits(context_);
        if (!logits) {
            throw std::runtime_error("llama_get_logits returned nullptr");
        }

        llama_token token_id = greedy_sample(logits, vocab_size, settings_.temperature);
        if (llama_vocab_is_eog(vocab_, token_id)) {
            break;
        }

        char buffer[kTokenPieceBufferBytes];
        const int n = llama_token_to_piece(vocab_, token_id, buffer, sizeof(buffer), 0, true);
        if (n < 0) {
            throw std::runtime_error("Failed to convert token to text piece");
        }
        response.append(buffer, n);

        llama_batch batch = llama_batch_get_one(&token_id, 1);
        if (llama_decode(context_, batch) != 0) {
            throw std::runtime_error("llama_decode failed during generation");
        }
    }

    if (diagnostics) {
        const auto generation_finished = std::chrono::steady_clock::now();
        diagnostics->generate_ms = elapsed_ms(generation_started, generation_finished);
        diagnostics->total_ms = elapsed_ms(inference_started, generation_finished);
    }

    return trim(response);
}
#else
std::string LlavaImageAnalyzer::infer_text(void* bitmap,
                                           std::string_view system_prompt,
                                           const std::string& user_prompt,
                                           int32_t max_tokens,
                                           ImageInferenceDiagnostics* diagnostics) {
    (void)bitmap;
    (void)system_prompt;
    (void)user_prompt;
    (void)max_tokens;
    (void)diagnostics;
    throw std::runtime_error("Visual LLM support is not available in this build.");
}
#endif

std::string LlavaImageAnalyzer::sanitize_filename(const std::string& value,
                                                  size_t max_words,
                                                  size_t max_length) const {
    QString cleaned = sanitize_utf8_text(value).trimmed();
    const QString prefix = QStringLiteral("filename:");
    if (cleaned.startsWith(prefix, Qt::CaseInsensitive)) {
        cleaned = cleaned.mid(prefix.size()).trimmed();
    }
    const int newline = cleaned.indexOf('\n');
    if (newline != -1) {
        cleaned = cleaned.left(newline);
    }
    const int carriage = cleaned.indexOf('\r');
    if (carriage != -1) {
        cleaned = cleaned.left(carriage);
    }
    if (cleaned.size() >= 2) {
        const QChar first = cleaned.front();
        const QChar last = cleaned.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            cleaned = cleaned.mid(1, cleaned.size() - 2);
        }
    }

    auto words = split_words(cleaned);
    std::vector<std::string> filtered;
    filtered.reserve(words.size());
    std::unordered_set<std::string> seen;
    for (const auto& word : words) {
        if (word.empty()) {
            continue;
        }
        if (kStopwords.find(word) != kStopwords.end()) {
            continue;
        }
        if (seen.insert(word).second) {
            filtered.push_back(word);
        }
        if (filtered.size() >= max_words) {
            break;
        }
    }

    if (filtered.empty()) {
        return std::string();
    }

    QString joined;
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (i > 0) {
            joined.append('_');
        }
        joined.append(QString::fromUtf8(filtered[i].c_str()));
    }

    if (joined.size() > static_cast<int>(max_length)) {
        joined = joined.left(static_cast<int>(max_length));
    }
    while (!joined.isEmpty() && joined.endsWith('_')) {
        joined.chop(1);
    }

    return joined.toUtf8().toStdString();
}

std::string LlavaImageAnalyzer::trim(std::string value) {
    return trim_copy(value);
}

std::string LlavaImageAnalyzer::slugify(const std::string& value) {
    const QString input = sanitize_utf8_text(value);
    QString slug;
    slug.reserve(input.size());
    bool last_sep = false;
    for (const QChar ch : input) {
        if (ch.isLetterOrNumber()) {
            slug.append(ch.toLower());
            last_sep = false;
        } else if (!last_sep && !slug.isEmpty()) {
            slug.append('_');
            last_sep = true;
        }
    }
    if (!slug.isEmpty() && slug.endsWith('_')) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = QStringLiteral("item");
    }
    return slug.toUtf8().toStdString();
}

std::string LlavaImageAnalyzer::normalize_filename(const std::string& base,
                                                   const std::filesystem::path& original_path) {
    const std::string ext = Utils::path_to_utf8(original_path.extension());
    if (base.empty()) {
        return Utils::path_to_utf8(original_path.filename());
    }
    return ext.empty() ? base : base + ext;
}
