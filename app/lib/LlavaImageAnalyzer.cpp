#include "LlavaImageAnalyzer.hpp"

#include "Logger.hpp"
#include "LlamaModelParams.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_set>

#ifdef AI_FILE_SORTER_HAS_MTMD
#include "ggml-backend.h"
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
MTMD_API void mtmd_helper_set_progress_callback(mtmd_progress_callback_t callback,
                                                void* user_data);
#endif
#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
MTMD_API void mtmd_helper_log_set(ggml_log_callback log_callback, void* user_data);
#endif
}
#endif

namespace {
constexpr size_t kMaxFilenameWords = 3;
constexpr size_t kMaxFilenameLength = 50;

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

std::vector<std::string> split_words(const std::string& value) {
    std::vector<std::string> words;
    std::string current;
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            current.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!current.empty()) {
            words.emplace_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) {
        words.emplace_back(std::move(current));
    }
    return words;
}

const std::unordered_set<std::string> kStopwords = {
    "a", "an", "and", "are", "as", "at", "based", "be", "by", "category", "describes",
    "description", "depicts", "details", "document", "file", "filename", "for", "from",
    "gif", "has", "image", "in", "is", "it", "jpeg", "jpg", "of", "on", "only",
    "photo", "picture", "png", "shows", "the", "this", "to", "txt", "unknown", "with"
};

#ifdef AI_FILE_SORTER_HAS_MTMD
struct BackendMemoryInfo {
    size_t free_bytes = 0;
    size_t total_bytes = 0;
    std::string name;
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

std::optional<BackendMemoryInfo> query_backend_memory(std::string_view backend_name) {
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

bool should_enable_mmproj_gpu(const std::filesystem::path& mmproj_path,
                              std::string_view backend_name,
                              const std::shared_ptr<spdlog::logger>& logger) {
    if (!case_insensitive_contains(backend_name, "vulkan")) {
        return true;
    }

    const auto memory = query_backend_memory("vulkan");
    if (!memory.has_value()) {
        if (logger) {
            logger->warn("Vulkan memory metrics unavailable; using CPU for visual encoder to avoid OOM. "
                         "Set AI_FILE_SORTER_VISUAL_USE_GPU=1 to force GPU.");
        }
        return false;
    }

    std::error_code ec;
    const auto file_size = std::filesystem::file_size(mmproj_path, ec);
    if (ec) {
        if (logger) {
            logger->warn("Failed to stat mmproj file '{}'; using CPU for visual encoder to avoid OOM. "
                         "Set AI_FILE_SORTER_VISUAL_USE_GPU=1 to force GPU.",
                         mmproj_path.string());
        }
        return false;
    }

    constexpr size_t kMinHeadroomBytes = 512ULL * 1024ULL * 1024ULL;
    const size_t mmproj_bytes = static_cast<size_t>(
        std::min<std::uintmax_t>(file_size, std::numeric_limits<size_t>::max()));
    const size_t inflated_bytes = mmproj_bytes + (mmproj_bytes / 3);
    const size_t required_bytes = inflated_bytes + kMinHeadroomBytes;

    if (memory->free_bytes < required_bytes) {
        if (logger) {
            const double to_mib = 1024.0 * 1024.0;
            logger->warn(
                "Vulkan free memory {:.1f} MiB < {:.1f} MiB needed for mmproj; using CPU for visual encoder. "
                "Set AI_FILE_SORTER_VISUAL_USE_GPU=1 to force GPU.",
                memory->free_bytes / to_mib,
                required_bytes / to_mib);
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
#endif

} // namespace

LlavaImageAnalyzer::LlavaImageAnalyzer(const std::filesystem::path& model_path,
                                       const std::filesystem::path& mmproj_path)
    : LlavaImageAnalyzer(model_path, mmproj_path, Settings{}) {}

LlavaImageAnalyzer::LlavaImageAnalyzer(const std::filesystem::path& model_path,
                                       const std::filesystem::path& mmproj_path,
                                       Settings settings)
#ifdef AI_FILE_SORTER_HAS_MTMD
    : model_path_(model_path)
    , mmproj_path_(mmproj_path)
    , settings_(settings)
#else
    : settings_(settings)
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

    auto logger = Logger::get_logger("core_logger");
    llama_model_params model_params = llama_model_default_params();
    if (settings_.use_gpu) {
        model_params = build_model_params_for_path(model_path.string(), logger);
    } else {
        model_params.n_gpu_layers = 0;
    }
    text_gpu_enabled_ = settings_.use_gpu && model_params.n_gpu_layers != 0;
    context_tokens_ = settings_.n_ctx;
    batch_size_ = 512;
    model_ = llama_model_load_from_file(model_path.string().c_str(), model_params);
    if (!model_) {
        throw std::runtime_error("Failed to load LLaVA text model at " + model_path.string());
    }

    vocab_ = llama_model_get_vocab(model_);

    bool mmproj_use_gpu = text_gpu_enabled_;
    if (mmproj_use_gpu && (!visual_gpu_override_.has_value() || !*visual_gpu_override_)) {
        mmproj_use_gpu = should_enable_mmproj_gpu(mmproj_path, resolve_backend_name(), logger);
    }
    mmproj_gpu_enabled_ = mmproj_use_gpu;

    mtmd_context_params mm_params = mtmd_context_params_default();
    mm_params.use_gpu = mmproj_gpu_enabled_;
    mm_params.n_threads = settings_.n_threads;
    vision_ctx_ = mtmd_init_from_file(mmproj_path.string().c_str(), model_, mm_params);
    if (!vision_ctx_) {
        llama_free(context_);
        llama_model_free(model_);
        context_ = nullptr;
        model_ = nullptr;
        throw std::runtime_error("Failed to load mmproj file at " + mmproj_path.string());
    }
    if (!mtmd_support_vision(vision_ctx_)) {
        throw std::runtime_error("The provided mmproj file does not expose vision capabilities");
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

bool LlavaImageAnalyzer::is_supported_image(const std::filesystem::path& path) {
    if (!path.has_extension()) {
        return false;
    }
    std::string ext = to_lower_copy(path.extension().string());
    static const std::unordered_set<std::string> kExtensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tga", ".psd", ".hdr",
        ".pic", ".pnm", ".ppm", ".pgm", ".pbm"
    };
    return kExtensions.find(ext) != kExtensions.end();
}

LlavaImageAnalysisResult LlavaImageAnalyzer::analyze(const std::filesystem::path& image_path) {
#ifndef AI_FILE_SORTER_HAS_MTMD
    (void)image_path;
    throw std::runtime_error("Visual LLM support is not available in this build.");
#else
    auto logger = Logger::get_logger("core_logger");
    BitmapPtr bitmap(mtmd_helper_bitmap_init_from_file(vision_ctx_, image_path.string().c_str()));
    if (!bitmap) {
        throw std::runtime_error("Failed to load image for LLaVA: " + image_path.string());
    }

    const std::string description = infer_text(bitmap.get(),
                                               build_description_prompt(),
                                               settings_.n_predict);

    const std::string raw_filename = infer_text(nullptr,
                                                build_filename_prompt(description),
                                                settings_.n_predict);
    if (logger) {
        logger->info("LLaVA raw filename: {}", raw_filename);
    }
    std::string filename_base = sanitize_filename(raw_filename, kMaxFilenameWords, kMaxFilenameLength);
    if (filename_base.empty()) {
        filename_base = sanitize_filename(description, kMaxFilenameWords, kMaxFilenameLength);
    }
    if (filename_base.empty()) {
        filename_base = "image_" + slugify(image_path.stem().string());
    }

    LlavaImageAnalysisResult result;
    result.description = description;
    result.suggested_name = normalize_filename(filename_base, image_path);
    if (logger) {
        logger->info("LLaVA suggested filename: {}", result.suggested_name);
    }
    return result;
#endif
}

std::string LlavaImageAnalyzer::build_description_prompt() const {
    std::ostringstream oss;
    oss << "Please provide a detailed description of this image, focusing on the main subject "
        << "and any important details.\n"
        << "Image: <__media__>\n"
        << "Description:";
    return oss.str();
}

std::string LlavaImageAnalyzer::build_filename_prompt(const std::string& description) const {
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
    return oss.str();
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
    if (user_data) {
        auto* self = static_cast<LlavaImageAnalyzer*>(user_data);
        if (self->settings_.batch_progress) {
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
    }
    std::fputs(text, stderr);
    std::fflush(stderr);
}
#endif

#endif

#ifdef AI_FILE_SORTER_HAS_MTMD
std::string LlavaImageAnalyzer::infer_text(mtmd_bitmap* bitmap,
                                           const std::string& prompt,
                                           int32_t max_tokens) {
    auto logger = Logger::get_logger("core_logger");
    const int32_t initial_ctx = context_tokens_ > 0 ? context_tokens_ : settings_.n_ctx;
    const int32_t initial_batch = batch_size_ > 0 ? batch_size_ : 512;

    auto try_init_context = [&](int32_t n_ctx, int32_t n_batch) -> bool {
        if (context_) {
            llama_free(context_);
            context_ = nullptr;
        }

        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = n_ctx;
        ctx_params.n_batch = std::min(n_batch, n_ctx);
        ctx_params.n_threads = settings_.n_threads;
        context_ = llama_init_from_model(model_, ctx_params);
        return context_ != nullptr;
    };

    auto apply_context_limits = [&](int32_t n_ctx, int32_t n_batch) {
        context_tokens_ = n_ctx;
        batch_size_ = std::min(n_batch, n_ctx);
    };

    bool context_ready = try_init_context(initial_ctx, initial_batch);
    if (context_ready) {
        apply_context_limits(initial_ctx, initial_batch);
    }
    if (!context_ready) {
        if (logger) {
            logger->warn("Failed to initialize llama_context (n_ctx={}, n_batch={}); retrying with smaller buffers.",
                         initial_ctx, initial_batch);
        }
        const int32_t reduced_ctx = std::min(initial_ctx, 2048);
        context_ready = try_init_context(reduced_ctx, initial_batch);
        if (context_ready) {
            apply_context_limits(reduced_ctx, initial_batch);
        } else {
            const int32_t reduced_batch = std::min(initial_batch, 256);
            context_ready = try_init_context(reduced_ctx, reduced_batch);
            if (context_ready) {
                apply_context_limits(reduced_ctx, reduced_batch);
            } else {
                const int32_t smaller_ctx = std::min(reduced_ctx, 1024);
                context_ready = try_init_context(smaller_ctx, reduced_batch);
                if (context_ready) {
                    apply_context_limits(smaller_ctx, reduced_batch);
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

    ChunkPtr chunks(mtmd_input_chunks_init());
    if (!chunks) {
        throw std::runtime_error("Failed to allocate mtmd input chunks");
    }

    mtmd_input_text text{};
    text.text = prompt.c_str();
    text.add_special = true;
    text.parse_special = true;

    const mtmd_bitmap* bitmaps[] = { bitmap };
    const mtmd_bitmap** bitmap_ptr = nullptr;
    int32_t bitmap_count = 0;
    if (bitmap) {
        bitmap_ptr = bitmaps;
        bitmap_count = 1;
    }

    const int32_t tokenize_res = mtmd_tokenize(
        vision_ctx_,
        chunks.get(),
        &text,
        bitmap_ptr,
        bitmap_count);
    if (tokenize_res != 0) {
        throw std::runtime_error("mtmd_tokenize failed with code " + std::to_string(tokenize_res));
    }

#if defined(AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK) || defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
    struct ProgressGuard {
        bool active{false};
        ProgressGuard(bool enabled, LlavaImageAnalyzer* self) : active(enabled) {
            if (!active) {
                return;
            }
#if defined(AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
            mtmd_helper_set_progress_callback(&LlavaImageAnalyzer::mtmd_progress_callback, self);
#elif defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
            mtmd_helper_log_set(&LlavaImageAnalyzer::mtmd_log_callback, self);
#endif
        }
        ~ProgressGuard() {
            if (!active) {
                return;
            }
#if defined(AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK)
            mtmd_helper_set_progress_callback(nullptr, nullptr);
#elif defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
            mtmd_helper_log_set(nullptr, nullptr);
#endif
        }
    };

    const bool enable_progress = bitmap && settings_.batch_progress;
    ProgressGuard progress_guard(enable_progress, this);
#endif

    llama_pos new_n_past = 0;
    if (mtmd_helper_eval_chunks(vision_ctx_,
                                context_,
                                chunks.get(),
                                0 /* n_past */,
                                0 /* seq_id */,
                                (batch_size_ > 0 ? batch_size_ : 512) /* n_batch */,
                                true /* logits_last */,
                                &new_n_past) != 0) {
        throw std::runtime_error("mtmd_helper_eval_chunks failed");
    }

    std::string response;
    response.reserve(256);

    const int vocab_size = llama_vocab_n_tokens(vocab_);
    for (int32_t i = 0; i < max_tokens; ++i) {
        const float* logits = llama_get_logits(context_);
        if (!logits) {
            throw std::runtime_error("llama_get_logits returned nullptr");
        }

        llama_token token_id = greedy_sample(logits, vocab_size, settings_.temperature);
        if (llama_vocab_is_eog(vocab_, token_id)) {
            break;
        }

        char buffer[256];
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

    return trim(response);
}
#else
std::string LlavaImageAnalyzer::infer_text(void* bitmap,
                                           const std::string& prompt,
                                           int32_t max_tokens) {
    (void)bitmap;
    (void)prompt;
    (void)max_tokens;
    throw std::runtime_error("Visual LLM support is not available in this build.");
}
#endif

std::string LlavaImageAnalyzer::sanitize_filename(const std::string& value,
                                                  size_t max_words,
                                                  size_t max_length) const {
    std::string cleaned = trim_copy(value);
    const std::string lower = to_lower_copy(cleaned);
    const std::string prefix = "filename:";
    if (lower.rfind(prefix, 0) == 0) {
        cleaned = trim_copy(cleaned.substr(prefix.size()));
    }
    const auto newline = cleaned.find('\n');
    if (newline != std::string::npos) {
        cleaned = cleaned.substr(0, newline);
    }
    if (cleaned.size() >= 2 && ((cleaned.front() == '"' && cleaned.back() == '"') ||
                                (cleaned.front() == '\'' && cleaned.back() == '\''))) {
        cleaned = cleaned.substr(1, cleaned.size() - 2);
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

    std::string joined;
    for (size_t i = 0; i < filtered.size(); ++i) {
        if (i > 0) {
            joined.push_back('_');
        }
        joined += filtered[i];
    }

    if (joined.size() > max_length) {
        joined.resize(max_length);
    }
    while (!joined.empty() && joined.back() == '_') {
        joined.pop_back();
    }

    return joined;
}

std::string LlavaImageAnalyzer::trim(std::string value) {
    return trim_copy(value);
}

std::string LlavaImageAnalyzer::slugify(const std::string& value) {
    std::string slug;
    slug.reserve(value.size());
    bool last_sep = false;
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            last_sep = false;
        } else if (!last_sep && !slug.empty()) {
            slug.push_back('_');
            last_sep = true;
        }
    }
    if (!slug.empty() && slug.back() == '_') {
        slug.pop_back();
    }
    if (slug.empty()) {
        slug = "item";
    }
    return slug;
}

std::string LlavaImageAnalyzer::normalize_filename(const std::string& base,
                                                   const std::filesystem::path& original_path) {
    const std::string ext = original_path.extension().string();
    if (base.empty()) {
        return original_path.filename().string();
    }
    return ext.empty() ? base : base + ext;
}
