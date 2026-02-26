#include "LocalLLMClient.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "TestHooks.hpp"
#include "LocalLLMTestAccess.hpp"
#include "llama.h"
#include "gguf.h"
#include "ggml-backend.h"
#include <string>
#include <vector>
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <regex>
#include <iostream>
#include <sstream>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <climits>
#include <cerrno>
#include <filesystem>
#include <optional>
#include <cmath>
#include <fstream>
#include <limits>
#include <string_view>
#include <string>
#include <array>
#include <utility>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

#if defined(_WIN32)
[[maybe_unused]] static void set_env_var(const char *key, const char *value) {
    _putenv_s(key, value);
}
#else
[[maybe_unused]] static void set_env_var(const char *key, const char *value) {
    setenv(key, value, 1);
}
#endif


namespace {

struct GgufCtxDeleter {
    void operator()(gguf_context* ctx) const { gguf_free(ctx); }
};

bool try_parse_env_int(const char *key, int &out) {
    const char *value = std::getenv(key);
    if (!value || *value == '\0') {
        return false;
    }

    char *end_ptr = nullptr;
    errno = 0;
    long parsed = std::strtol(value, &end_ptr, 10);
    if (end_ptr == value || *end_ptr != '\0' || errno == ERANGE) {
        return false;
    }
    if (parsed > INT_MAX || parsed < INT_MIN) {
        return false;
    }

    out = static_cast<int>(parsed);
    return true;
}

int resolve_gpu_layer_override() {
    int parsed = 0;
    if (try_parse_env_int("AI_FILE_SORTER_N_GPU_LAYERS", parsed)) {
        return parsed;
    }
    if (try_parse_env_int("LLAMA_CPP_N_GPU_LAYERS", parsed)) {
        return parsed;
    }
    return INT_MIN;
}

std::string gpu_layers_to_string(int value) {
    if (value == -1) {
        return "auto (-1)";
    }
    return std::to_string(value);
}

int resolve_context_length() {
    int parsed = 0;
    if (try_parse_env_int("AI_FILE_SORTER_CTX_TOKENS", parsed) && parsed > 0) {
        return parsed;
    }
    if (try_parse_env_int("LLAMA_CPP_MAX_CONTEXT", parsed) && parsed > 0) {
        return parsed;
    }
    return 2048; // increased default to better accommodate larger prompts (whitelists, hints)
}

bool is_cpu_backend_requested() {
    auto is_cpu_env = [](const char* value) {
        if (!value || *value == '\0') {
            return false;
        }
        std::string lowered(value);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lowered == "cpu";
    };

    if (is_cpu_env(std::getenv("AI_FILE_SORTER_GPU_BACKEND")) ||
        is_cpu_env(std::getenv("LLAMA_ARG_DEVICE"))) {
        return true;
    }

    const int override_layers = resolve_gpu_layer_override();
    return override_layers != INT_MIN && override_layers <= 0;
}

bool allow_gpu_fallback(const LocalLLMClient::FallbackDecisionCallback& callback,
                        const std::shared_ptr<spdlog::logger>& logger,
                        std::string_view reason)
{
    if (is_cpu_backend_requested()) {
        return false;
    }
    if (!callback) {
        return true;
    }
    bool allowed = callback(std::string(reason));
    if (!allowed && logger) {
        logger->warn("GPU fallback declined: {}", reason);
    }
    return allowed;
}

struct MetalDeviceInfo {
    size_t total_bytes = 0;
    size_t free_bytes = 0;
    std::string name;

    bool valid() const {
        return total_bytes > 0;
    }
};

#if defined(GGML_USE_METAL)
MetalDeviceInfo query_primary_gpu_device() {
    MetalDeviceInfo info;

#if defined(__APPLE__)
    uint64_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0) == 0) {
        info.total_bytes = static_cast<size_t>(memsize);
    }

    mach_port_t host_port = mach_host_self();
    vm_size_t page_size = 0;
    if (host_port != MACH_PORT_NULL && host_page_size(host_port, &page_size) == KERN_SUCCESS) {
        vm_statistics64_data_t vm_stat {};
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(host_port, HOST_VM_INFO64,
                              reinterpret_cast<host_info64_t>(&vm_stat), &count) == KERN_SUCCESS) {
            const uint64_t free_pages = static_cast<uint64_t>(vm_stat.free_count) +
                                        static_cast<uint64_t>(vm_stat.inactive_count);
            info.free_bytes = static_cast<size_t>(free_pages * static_cast<uint64_t>(page_size));
        }
    }

    info.name = "Metal (system memory)";
#endif

    return info;
}

bool metal_backend_available(const std::shared_ptr<spdlog::logger>& logger) {
    ggml_backend_reg_t metal = ggml_backend_reg_by_name("Metal");
    if (!metal) {
        if (logger) {
            logger->warn("Metal backend not registered; falling back to CPU");
        }
        return false;
    }
    const size_t dev_count = ggml_backend_reg_dev_count(metal);
    if (dev_count == 0) {
        if (logger) {
            logger->warn("No Metal devices detected; falling back to CPU");
        }
        return false;
    }
    return true;
}
#endif // defined(GGML_USE_METAL)

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

bool is_probably_integrated_gpu(ggml_backend_dev_t device,
                                enum ggml_backend_dev_type type) {
#if defined(AI_FILE_SORTER_GGML_HAS_IGPU_ENUM)
    (void) device;
    return type == GGML_BACKEND_DEVICE_TYPE_IGPU;
#else
    (void) type;

    const auto matches_hint = [](const char * value) {
        if (!value || value[0] == '\0') {
            return false;
        }
        constexpr std::array<std::string_view, 4> hints = {"integrated", "apu", "shared", "uma"};
        const std::string_view view(value);
        for (const std::string_view hint : hints) {
            if (case_insensitive_contains(view, hint)) {
                return true;
            }
        }
        return false;
    };

    return matches_hint(ggml_backend_dev_name(device)) ||
           matches_hint(ggml_backend_dev_description(device));
#endif
}

void load_ggml_backends_once(const std::shared_ptr<spdlog::logger>& logger) {
    static bool loaded = false;
    if (loaded) {
        return;
    }

    const char* ggml_dir = std::getenv("AI_FILE_SORTER_GGML_DIR");
    if (ggml_dir && ggml_dir[0] != '\0') {
        if (logger) {
            logger->info("Loading ggml backends from '{}'", ggml_dir);
        }
        ggml_backend_load_all_from_path(ggml_dir);
    } else {
        ggml_backend_load_all();
    }

    loaded = true;
}

using BackendMemoryInfo = TestHooks::BackendMemoryInfo;

using BackendMemoryProbe = TestHooks::BackendMemoryProbe;
using BackendAvailabilityProbe = TestHooks::BackendAvailabilityProbe;

BackendMemoryProbe& backend_memory_probe_slot() {
    static BackendMemoryProbe probe;
    return probe;
}

BackendAvailabilityProbe& backend_availability_probe_slot() {
    static BackendAvailabilityProbe probe;
    return probe;
}

bool query_backend_available_impl(std::string_view backend_name) {
    if (backend_name.empty()) {
        return false;
    }

    const std::string backend_name_str(backend_name);
    ggml_backend_reg_t backend_reg = ggml_backend_reg_by_name(backend_name_str.c_str());
    if (!backend_reg) {
        return false;
    }

    return ggml_backend_reg_dev_count(backend_reg) > 0;
}

bool resolve_backend_available(std::string_view backend_name) {
    if (auto& probe = backend_availability_probe_slot()) {
        return probe(backend_name);
    }
    return query_backend_available_impl(backend_name);
}

bool backend_name_matches(const char *name, std::string_view backend_name) {
    if (backend_name.empty()) {
        return true;
    }
    return name && case_insensitive_contains(name, backend_name);
}

std::optional<BackendMemoryInfo> build_backend_memory_info(ggml_backend_dev_t device,
                                                           std::string_view backend_name) {
    if (!device) {
        return std::nullopt;
    }

    const auto type = ggml_backend_dev_type(device);
    if (type != GGML_BACKEND_DEVICE_TYPE_GPU) {
        return std::nullopt;
    }

    auto * reg = ggml_backend_dev_backend_reg(device);
    const char * name = reg ? ggml_backend_reg_name(reg) : nullptr;
    if (!backend_name_matches(name, backend_name)) {
        return std::nullopt;
    }

    size_t free_bytes = 0;
    size_t total_bytes = 0;
    ggml_backend_dev_memory(device, &free_bytes, &total_bytes);
    if (free_bytes == 0 && total_bytes == 0) {
        return std::nullopt;
    }

    BackendMemoryInfo info;
    info.memory.free_bytes = free_bytes;
    info.memory.total_bytes = (total_bytes != 0) ? total_bytes : free_bytes;
    info.is_integrated = is_probably_integrated_gpu(device, type);
    info.name = name ? name : "";

    return info;
}

std::optional<BackendMemoryInfo> query_backend_memory_metrics_impl(std::string_view backend_name) {
    const size_t device_count = ggml_backend_dev_count();
    BackendMemoryInfo best{};
    bool found = false;

    for (size_t i = 0; i < device_count; ++i) {
        auto * device = ggml_backend_dev_get(i);
        const auto info = build_backend_memory_info(device, backend_name);
        if (!info.has_value()) {
            continue;
        }
        if (!found || info->memory.total_bytes > best.memory.total_bytes) {
            best = *info;
            found = true;
        }
    }

    if (found) {
        return best;
    }
    return std::nullopt;
}

[[maybe_unused]] std::optional<BackendMemoryInfo> resolve_backend_memory(std::string_view backend_name) {
    if (auto& probe = backend_memory_probe_slot()) {
        return probe(backend_name);
    }
    return query_backend_memory_metrics_impl(backend_name);
}

} // namespace

namespace TestHooks {

void set_backend_memory_probe(BackendMemoryProbe probe) {
    backend_memory_probe_slot() = std::move(probe);
}

void reset_backend_memory_probe() {
    backend_memory_probe_slot() = BackendMemoryProbe{};
}

void set_backend_availability_probe(BackendAvailabilityProbe probe) {
    backend_availability_probe_slot() = std::move(probe);
}

void reset_backend_availability_probe() {
    backend_availability_probe_slot() = BackendAvailabilityProbe{};
}

} // namespace TestHooks

namespace {

bool read_file_prefix(std::ifstream& file,
                      std::vector<char>& buffer,
                      std::size_t requested_bytes,
                      std::size_t& bytes_read)
{
    const auto compute_safe_request = [&](std::size_t& safe_request) -> bool {
        if (requested_bytes == 0 || requested_bytes > buffer.size()) {
            return false;
        }
        const auto max_streamsize = static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max());
        const std::size_t clamped_request = std::min(requested_bytes, buffer.size());
        safe_request = std::min(clamped_request, max_streamsize);
        return safe_request > 0;
    };

    const auto validate_read_count = [&](std::streamsize read_count, std::size_t to_request) -> bool {
        if (read_count <= 0) {
            return false;
        }
        if (read_count > static_cast<std::streamsize>(to_request) ||
            static_cast<std::size_t>(read_count) > buffer.size()) {
            return false;
        }
        return true;
    };

    std::size_t safe_request = 0;
    if (!compute_safe_request(safe_request)) {
        return false;
    }

    const std::streamsize to_request = static_cast<std::streamsize>(safe_request);
    file.read(buffer.data(), to_request);
    if (!file && !file.eof()) {
        return false;
    }

    const std::streamsize read_count = file.gcount();
    if (!validate_read_count(read_count, safe_request)) {
        return false;
    }

    bytes_read = static_cast<std::size_t>(read_count);
    return true;
}

uint32_t read_le32(const char* ptr)
{
    uint32_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint32_t));
    return value;
}

uint64_t read_le64(const char* ptr)
{
    uint64_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint64_t));
    return value;
}

std::optional<int32_t> read_uint_value(uint32_t type,
                                       const char* ptr,
                                       std::size_t available_bytes)
{
    switch (type) {
        case 4: // GGUF_TYPE_UINT32
        case 5: // GGUF_TYPE_INT32:
            if (available_bytes >= sizeof(uint32_t)) {
                return static_cast<int32_t>(read_le32(ptr));
            }
            break;
        case 10: // GGUF_TYPE_UINT64
        case 11: // GGUF_TYPE_INT64
            if (available_bytes >= sizeof(uint64_t)) {
                return static_cast<int32_t>(read_le64(ptr));
            }
            break;
        default:
            break;
    }
    return std::nullopt;
}

std::optional<int32_t> read_gguf_numeric(gguf_context* ctx, int64_t id)
{
    const enum gguf_type type = gguf_get_kv_type(ctx, id);
    switch (type) {
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

std::optional<int32_t> try_block_count_keys(gguf_context* ctx,
                                            const std::array<const char*, 6>& keys)
{
    for (const char* key : keys) {
        const int64_t id = gguf_find_key(ctx, key);
        if (id < 0) {
            continue;
        }
        if (auto value = read_gguf_numeric(ctx, id)) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<int32_t> infer_block_count_from_tensors(gguf_context* ctx)
{
    const int64_t tensor_count = gguf_get_n_tensors(ctx);
    int32_t max_layer = -1;
    for (int64_t i = 0; i < tensor_count; ++i) {
        const char* tname = gguf_get_tensor_name(ctx, i);
        if (!tname) {
            continue;
        }
        int32_t current = -1;
        for (const char* p = tname; *p; ++p) {
            if (std::isdigit(static_cast<unsigned char>(*p))) {
                int value = 0;
                while (*p && std::isdigit(static_cast<unsigned char>(*p))) {
                    value = value * 10 + (*p - '0');
                    ++p;
                }
                current = std::max(current, value);
            }
        }
        if (current > max_layer) {
            max_layer = current;
        }
    }
    if (max_layer >= 0) {
        return max_layer + 1; // zero-indexed
    }
    return std::nullopt;
}

bool format_prompt(llama_model* model, const std::string& prompt, std::string& final_prompt)
{
    std::vector<llama_chat_message> messages;
    messages.push_back({"user", prompt.c_str()});
    const char* tmpl = llama_model_chat_template(model, nullptr);
    std::vector<char> formatted_prompt(8192);

    int actual_len = llama_chat_apply_template(tmpl,
                                               messages.data(),
                                               messages.size(),
                                               true,
                                               formatted_prompt.data(),
                                               formatted_prompt.size());
    if (actual_len < 0) {
        return false;
    }

    final_prompt.assign(formatted_prompt.data(), static_cast<size_t>(actual_len));
    return true;
}

bool tokenize_prompt(const llama_vocab* vocab,
                     const std::string& final_prompt,
                     std::vector<llama_token>& prompt_tokens,
                     int& n_prompt,
                     const std::shared_ptr<spdlog::logger>& logger)
{
    n_prompt = -llama_tokenize(vocab,
                               final_prompt.c_str(),
                               final_prompt.size(),
                               nullptr,
                               0,
                               true,
                               true);
    if (n_prompt <= 0) {
        if (logger) {
            logger->error("Failed to determine token count for prompt");
        }
        return false;
    }

    prompt_tokens.resize(static_cast<size_t>(n_prompt));
    if (llama_tokenize(vocab,
                       final_prompt.c_str(),
                       final_prompt.size(),
                       prompt_tokens.data(),
                       prompt_tokens.size(),
                       true,
                       true) < 0) {
        if (logger) {
            logger->error("Tokenization failed for prompt");
        }
        return false;
    }

    return true;
}

std::string run_generation_loop(llama_context* ctx,
                                llama_sampler* smpl,
                                std::vector<llama_token>& prompt_tokens,
                                int n_prompt,
                                int max_tokens,
                                const std::shared_ptr<spdlog::logger>& logger,
                                const llama_vocab* vocab)
{
    const int ctx_n_ctx = static_cast<int>(llama_n_ctx(ctx));
    int ctx_n_batch = static_cast<int>(llama_n_batch(ctx));
    if (ctx_n_batch <= 0) {
        ctx_n_batch = ctx_n_ctx;
    }

    if (ctx_n_ctx > 0 && n_prompt > ctx_n_ctx) {
        const int overflow = n_prompt - ctx_n_ctx;
        if (overflow > 0 && overflow < n_prompt) {
            if (logger) {
                logger->warn("Prompt tokens ({}) exceed context ({}) by {}; truncating oldest tokens",
                             n_prompt, ctx_n_ctx, overflow);
            }
            prompt_tokens.erase(prompt_tokens.begin(),
                                prompt_tokens.begin() + overflow);
            n_prompt = ctx_n_ctx;
        }
    }

    int n_pos = 0;
    while (n_pos < n_prompt) {
        const int chunk = std::min(ctx_n_batch, n_prompt - n_pos);
        llama_batch batch = llama_batch_get_one(prompt_tokens.data() + n_pos, chunk);
        if (llama_decode(ctx, batch)) {
            if (logger) {
                logger->warn("llama_decode returned non-zero status during prompt eval; aborting generation");
            }
            return std::string();
        }
        n_pos += chunk;
    }

    std::string output;
    int generated_tokens = 0;
    while (generated_tokens < max_tokens) {
        llama_token new_token_id = llama_sampler_sample(smpl, ctx, -1);
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }

        char buf[128];
        int n = llama_token_to_piece(vocab, new_token_id, buf,
                                     sizeof(buf), 0, true);
        if (n < 0) {
            break;
        }
        output.append(buf, n);
        generated_tokens++;

        llama_batch batch = llama_batch_get_one(&new_token_id, 1);
        if (llama_decode(ctx, batch)) {
            if (logger) {
                logger->warn("llama_decode returned non-zero status; aborting generation");
            }
            break;
        }
    }

    while (!output.empty() && std::isspace(static_cast<unsigned char>(output.front()))) {
        output.erase(output.begin());
    }

    return output;
}

std::optional<int32_t> parse_block_count_entry(const std::vector<char>& buffer,
                                               std::size_t bytes_read,
                                               std::size_t key_pos,
                                               std::string_view key)
{
    if (key_pos < sizeof(uint64_t)) {
        return std::nullopt;
    }

    const uint64_t declared_len = read_le64(buffer.data() + key_pos - sizeof(uint64_t));
    if (declared_len != key.size()) {
        return std::nullopt;
    }

    const std::size_t type_offset = key_pos + key.size();
    if (type_offset + sizeof(uint32_t) > bytes_read) {
        return std::nullopt;
    }

    const uint32_t type = read_le32(buffer.data() + type_offset);
    const std::size_t value_offset = type_offset + sizeof(uint32_t);
    if (value_offset >= bytes_read) {
        return std::nullopt;
    }

    const std::size_t available = bytes_read - value_offset;
    return read_uint_value(type, buffer.data() + value_offset, available);
}

std::optional<int32_t> extract_block_count_gguf(const std::string& model_path) {
    gguf_init_params params{};
    params.no_alloc = true;
    gguf_context* ctx = gguf_init_from_file(model_path.c_str(), params);
    if (!ctx) {
        return std::nullopt;
    }

    auto cleanup = std::unique_ptr<gguf_context, GgufCtxDeleter>(ctx);
    static const std::array<const char*, 6> block_keys = {
        "llama.block_count",
        "llama.layer_count",
        "llama.n_layer",
        "qwen.block_count",
        "qwen2.block_count",
        "block_count"
    };

    if (auto meta_val = try_block_count_keys(ctx, block_keys)) {
        return meta_val;
    }

    if (auto inferred = infer_block_count_from_tensors(ctx)) {
        return inferred;
    }

    return std::nullopt;
}

std::optional<int32_t> extract_block_count(const std::string & model_path) {
    if (const auto via_ctx = extract_block_count_gguf(model_path)) {
        return via_ctx;
    }
    std::ifstream file(model_path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    constexpr std::size_t kScanBytes = 8 * 1024 * 1024; // first 8 MiB should contain metadata
    std::vector<char> buffer(kScanBytes);

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
    if (!read_file_prefix(file, buffer, static_cast<std::size_t>(to_read), bytes_read)) {
        return std::nullopt;
    }

    const std::string_view data(buffer.data(), bytes_read);
    [[maybe_unused]] static const std::string_view candidate_keys[] = {
        "llama.block_count",
        "llama.layer_count",
        "llama.n_layer",
        "qwen.block_count",
        "qwen2.block_count",
        "block_count",
    };

    for (const auto & key : candidate_keys) {
        std::size_t pos = data.find(key);
        while (pos != std::string_view::npos) {
            if (const auto parsed_value = parse_block_count_entry(buffer, bytes_read, pos, key)) {
                return parsed_value;
            }

            pos = data.find(key, pos + 1);
        }
    }

    return std::nullopt;
}

struct AutoGpuLayerEstimation {
    int32_t layers = -1;
    std::string reason;
};

#if defined(GGML_USE_METAL)
AutoGpuLayerEstimation estimate_gpu_layers_for_metal(const std::string & model_path,
                                                     const MetalDeviceInfo & device_info) {
    AutoGpuLayerEstimation result;

    if (!device_info.valid()) {
        result.layers = -1;
        result.reason = "no GPU memory metrics available";
        return result;
    }

    std::error_code ec;
    const auto file_size = std::filesystem::file_size(model_path, ec);
    if (ec) {
        result.layers = -1;
        result.reason = "model file size unavailable";
        return result;
    }

    const auto block_count_opt = extract_block_count(model_path);
    if (!block_count_opt.has_value() || block_count_opt.value() <= 0) {
        result.layers = -1;
        result.reason = "model block count not found";
        return result;
    }

    const int32_t total_layers = block_count_opt.value();
    const double bytes_per_layer = static_cast<double>(file_size) / static_cast<double>(total_layers);

    // Prefer reported free memory, but fall back to a fraction of total RAM on unified-memory systems.
    double approx_free = static_cast<double>(device_info.free_bytes);
    double total_bytes = static_cast<double>(device_info.total_bytes);

    if (approx_free <= 0.0) {
        approx_free = total_bytes * 0.6; // assume we can use ~60% of total RAM when free info is missing
    }

    const double safety_reserve = std::max(total_bytes * 0.10, 512.0 * 1024.0 * 1024.0); // keep at least 10% or 512 MiB free
    double budget_bytes = std::max(approx_free - safety_reserve, total_bytes * 0.35);    // use at least 35% of total as budget
    budget_bytes = std::min(budget_bytes, total_bytes * 0.80);                           // never try to use more than 80% of RAM

    if (budget_bytes <= 0.0 || bytes_per_layer <= 0.0) {
        result.layers = 0;
        result.reason = "insufficient GPU memory budget";
        return result;
    }

    // Account for temporary buffers / fragmentation.
    const double overhead_factor = 1.20;
    int32_t estimated_layers = static_cast<int32_t>(std::floor(budget_bytes / (bytes_per_layer * overhead_factor)));
    estimated_layers = std::clamp<int32_t>(estimated_layers, 1, total_layers);

    result.layers = estimated_layers;
    if (estimated_layers == 0) {
        result.reason = "model layers larger than available GPU headroom";
    } else {
        result.reason = "estimated from GPU memory headroom";
    }

    return result;
}
#endif // defined(GGML_USE_METAL)

namespace {

struct LayerMetrics {
    int32_t total_layers{0};
    double bytes_per_layer{0.0};
};

bool populate_layer_metrics(const std::string& model_path,
                            AutoGpuLayerEstimation& result,
                            LayerMetrics& metrics)
{
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(model_path, ec);
    if (ec) {
        result.layers = -1;
        result.reason = "model file size unavailable";
        return false;
    }

    const auto block_count_opt = extract_block_count(model_path);
    if (!block_count_opt.has_value() || block_count_opt.value() <= 0) {
        result.layers = -1;
        result.reason = "model block count not found";
        return false;
    }

    metrics.total_layers = block_count_opt.value();
    metrics.bytes_per_layer =
        static_cast<double>(file_size) / static_cast<double>(metrics.total_layers);
    return true;
}

struct CudaBudget {
    double approx_free{0.0};
    double usable_total{0.0};
    double budget_bytes{0.0};
};

bool compute_cuda_budget(const Utils::CudaMemoryInfo& memory_info,
                         double bytes_per_layer,
                         AutoGpuLayerEstimation& result,
                         CudaBudget& budget)
{
    if (!memory_info.valid()) {
        result.layers = -1;
        result.reason = "CUDA memory metrics unavailable";
        return false;
    }

    budget.approx_free = static_cast<double>(memory_info.free_bytes);
    double total_bytes = static_cast<double>(memory_info.total_bytes);
    if (total_bytes <= 0.0) {
        total_bytes = budget.approx_free;
    }

    budget.usable_total = std::max(total_bytes, budget.approx_free);
    if (budget.usable_total <= 0.0) {
        result.layers = 0;
        result.reason = "CUDA memory metrics invalid";
        return false;
    }

    if (budget.approx_free <= 0.0) {
        budget.approx_free = budget.usable_total * 0.80;
    } else if (budget.approx_free > budget.usable_total) {
        budget.approx_free = budget.usable_total;
    }

    if (budget.approx_free <= 0.0 || bytes_per_layer <= 0.0) {
        result.layers = 0;
        result.reason = "insufficient CUDA memory metrics";
        return false;
    }

    const double safety_reserve =
        std::max(budget.usable_total * 0.05, 192.0 * 1024.0 * 1024.0);
    budget.budget_bytes = budget.approx_free - safety_reserve;
    if (budget.budget_bytes <= 0.0) {
        budget.budget_bytes = budget.approx_free * 0.75;
    }

    const double max_budget = std::min(budget.approx_free * 0.98, budget.usable_total * 0.90);
    const double min_budget = budget.usable_total * 0.45;
    budget.budget_bytes = std::clamp(budget.budget_bytes, min_budget, max_budget);
    return true;
}

bool finalize_cuda_estimate(const LayerMetrics& metrics,
                            const CudaBudget& budget,
                            AutoGpuLayerEstimation& result)
{
    constexpr double overhead_factor = 1.08;
    const double denominator = metrics.bytes_per_layer * overhead_factor;
    if (denominator <= 0.0) {
        result.layers = 0;
        result.reason = "invalid CUDA layer parameters";
        return false;
    }

    int32_t estimated_layers =
        static_cast<int32_t>(std::floor(budget.budget_bytes / denominator));
    if (estimated_layers <= 0) {
        result.layers = 0;
        result.reason = "insufficient CUDA memory budget";
        return false;
    }

    result.layers = std::clamp<int32_t>(estimated_layers, 1, metrics.total_layers);
    result.reason = "estimated from CUDA memory headroom";
    return true;
}

} // namespace

[[maybe_unused]] AutoGpuLayerEstimation estimate_gpu_layers_for_cuda(const std::string & model_path,
                                                                     const Utils::CudaMemoryInfo & memory_info) {
    AutoGpuLayerEstimation result;

    LayerMetrics metrics;
    if (!populate_layer_metrics(model_path, result, metrics)) {
        return result;
    }

    CudaBudget budget;
    if (!compute_cuda_budget(memory_info, metrics.bytes_per_layer, result, budget)) {
        return result;
    }

    if (!finalize_cuda_estimate(metrics, budget, result)) {
        return result;
    }

    return result;
}

enum class PreferredBackend {
    Auto,
    Cpu,
    Cuda,
    Vulkan
};

[[maybe_unused]] PreferredBackend detect_preferred_backend() {
    const char* env = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
    if (!env || *env == '\0') {
        return PreferredBackend::Auto;
    }
    std::string value(env);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (value == "cuda") {
        return PreferredBackend::Cuda;
    }
    if (value == "vulkan") {
        return PreferredBackend::Vulkan;
    }
    if (value == "cpu") {
        return PreferredBackend::Cpu;
    }
    return PreferredBackend::Auto;
}

#ifdef GGML_USE_METAL
int determine_metal_layers(const std::string& model_path,
                           const std::shared_ptr<spdlog::logger>& logger) {
    int gpu_layers = resolve_gpu_layer_override();
    if (gpu_layers == INT_MIN) {
        const MetalDeviceInfo device_info = query_primary_gpu_device();
        const auto estimation = estimate_gpu_layers_for_metal(model_path, device_info);

        gpu_layers = (estimation.layers >= 0) ? estimation.layers : -1;

        if (logger) {
            const double to_mib = 1024.0 * 1024.0;
            logger->info(
                "Metal device '{}' total {:.1f} MiB, free {:.1f} MiB -> n_gpu_layers={} ({})",
                device_info.name.empty() ? "GPU" : device_info.name,
                device_info.total_bytes / to_mib,
                device_info.free_bytes / to_mib,
                gpu_layers_to_string(gpu_layers),
                estimation.reason
            );
        }
    } else if (logger) {
        logger->info("Using Metal backend with explicit n_gpu_layers override={}",
                     gpu_layers_to_string(gpu_layers));
    }

    return gpu_layers;
}
#else
bool apply_cpu_backend(llama_model_params& params,
                       PreferredBackend backend_pref,
                       const std::shared_ptr<spdlog::logger>& logger) {
    if (backend_pref != PreferredBackend::Cpu) {
        return false;
    }
    params.n_gpu_layers = 0;
    set_env_var("GGML_DISABLE_CUDA", "1");
    if (logger) {
        logger->info("GPU backend disabled via AI_FILE_SORTER_GPU_BACKEND=cpu");
    }
    return true;
}

bool apply_vulkan_override(llama_model_params& params,
                           int override_layers,
                           const std::shared_ptr<spdlog::logger>& logger)
{
    if (override_layers == INT_MIN) {
        return false;
    }

    if (override_layers <= 0) {
        params.n_gpu_layers = 0;
        if (logger) {
            logger->info("Vulkan backend requested but AI_FILE_SORTER_N_GPU_LAYERS <= 0; using CPU instead.");
        }
        return true;
    }

    params.n_gpu_layers = override_layers;
    if (logger) {
        logger->info("Using Vulkan backend with explicit n_gpu_layers override={}",
                     gpu_layers_to_string(override_layers));
    }
    return true;
}

Utils::CudaMemoryInfo cap_integrated_gpu_memory(const BackendMemoryInfo& backend_memory,
                                                const std::shared_ptr<spdlog::logger>& logger)
{
    Utils::CudaMemoryInfo adjusted = backend_memory.memory;
    if (!backend_memory.is_integrated) {
        return adjusted;
    }

    constexpr size_t igpu_cap_bytes = static_cast<size_t>(4ULL) * 1024ULL * 1024ULL * 1024ULL; // 4 GiB
    adjusted.free_bytes = std::min(adjusted.free_bytes, igpu_cap_bytes);
    adjusted.total_bytes = std::min(adjusted.total_bytes, igpu_cap_bytes);
    if (logger) {
        const double to_mib = 1024.0 * 1024.0;
        logger->info("Vulkan device reported as integrated GPU; capping usable memory to {:.1f} MiB",
                     igpu_cap_bytes / to_mib);
    }
    return adjusted;
}

void log_vulkan_estimation(const Utils::CudaMemoryInfo& memory,
                           const BackendMemoryInfo& original,
                           const AutoGpuLayerEstimation& estimation,
                           int resolved_layers,
                           const std::shared_ptr<spdlog::logger>& logger)
{
    if (!logger) {
        return;
    }
    const double to_mib = 1024.0 * 1024.0;
    const char* device_label = original.name.empty() ? "Vulkan device" : original.name.c_str();
    logger->info(
        "{} total {:.1f} MiB, free {:.1f} MiB -> n_gpu_layers={} ({})",
        device_label,
        memory.total_bytes / to_mib,
        memory.free_bytes / to_mib,
        gpu_layers_to_string(resolved_layers),
        estimation.reason.empty() ? "auto-estimated" : estimation.reason);
}

bool finalize_vulkan_layers(const AutoGpuLayerEstimation& estimation,
                            const Utils::CudaMemoryInfo& memory,
                            llama_model_params& params,
                            const BackendMemoryInfo& original,
                            const std::shared_ptr<spdlog::logger>& logger)
{
    if (estimation.layers > 0) {
        params.n_gpu_layers = estimation.layers;
        log_vulkan_estimation(memory, original, estimation, params.n_gpu_layers, logger);
        return true;
    }

    params.n_gpu_layers = -1;
    if (logger) {
        logger->warn(
            "Vulkan estimator could not determine n_gpu_layers ({}); leaving llama.cpp auto (-1).",
            estimation.reason.empty() ? "no detail" : estimation.reason);
    }
    return true;
}

bool apply_vulkan_backend(const std::string& model_path,
                          llama_model_params& params,
                          const std::shared_ptr<spdlog::logger>& logger) {
    load_ggml_backends_once(logger);
    set_env_var("GGML_DISABLE_CUDA", "1");

    if (!resolve_backend_available("Vulkan")) {
        params.n_gpu_layers = 0;
        set_env_var("AI_FILE_SORTER_GPU_BACKEND", "cpu");
        set_env_var("LLAMA_ARG_DEVICE", "cpu");
        if (logger) {
            logger->warn("Vulkan backend unavailable; using CPU backend.");
        }
        return false;
    }

    const auto vk_memory = resolve_backend_memory("vulkan");
    if (apply_vulkan_override(params, resolve_gpu_layer_override(), logger)) {
        return true;
    }

    if (!vk_memory.has_value()) {
        params.n_gpu_layers = 0;
        set_env_var("AI_FILE_SORTER_GPU_BACKEND", "cpu");
        set_env_var("LLAMA_ARG_DEVICE", "cpu");
        if (logger) {
            logger->warn("Vulkan backend memory metrics unavailable; using CPU backend.");
        }
        return false;
    }

    Utils::CudaMemoryInfo adjusted = cap_integrated_gpu_memory(*vk_memory, logger);
    const auto estimation = estimate_gpu_layers_for_cuda(model_path, adjusted);
    finalize_vulkan_layers(estimation, adjusted, params, *vk_memory, logger);
    return true;
}

bool handle_cuda_forced_off(bool cuda_forced_off,
                            PreferredBackend backend_pref,
                            llama_model_params& params,
                            const std::shared_ptr<spdlog::logger>& logger) {
    if (!cuda_forced_off) {
        return false;
    }
    params.n_gpu_layers = 0;
    set_env_var("GGML_DISABLE_CUDA", "1");
    if (logger) {
        logger->info("CUDA disabled via GGML_DISABLE_CUDA environment override.");
        if (backend_pref == PreferredBackend::Cuda) {
            logger->warn("AI_FILE_SORTER_GPU_BACKEND=cuda but GGML_DISABLE_CUDA forces CPU fallback.");
        }
    }
    return true;
}

void disable_cuda_backend(llama_model_params& params,
                          const std::shared_ptr<spdlog::logger>& logger,
                          const std::string& reason)
{
    params.n_gpu_layers = 0;
    set_env_var("GGML_DISABLE_CUDA", "1");
    if (logger) {
        logger->info("CUDA backend disabled: {}", reason);
    }
}

bool ensure_cuda_available(llama_model_params& params,
                           const std::shared_ptr<spdlog::logger>& logger)
{
    if (Utils::is_cuda_available()) {
        return true;
    }
    disable_cuda_backend(params, logger, "CUDA backend unavailable; using CPU backend");
    std::cout << "No supported GPU backend detected. Running on CPU.\n";
    return false;
}

bool apply_ngl_override(int override_layers,
                        llama_model_params& params,
                        const std::shared_ptr<spdlog::logger>& logger)
{
    if (override_layers == INT_MIN) {
        return false;
    }

    if (override_layers <= 0) {
        disable_cuda_backend(
            params,
            logger,
            fmt::format("AI_FILE_SORTER_N_GPU_LAYERS={} forcing CPU fallback", override_layers));
        return true;
    }

    params.n_gpu_layers = override_layers;
    if (logger) {
        logger->info("Using explicit CUDA n_gpu_layers override {}",
                     gpu_layers_to_string(override_layers));
    }
    std::cout << "ngl override: " << params.n_gpu_layers << std::endl;
    return true;
}

struct NglEstimationResult {
    int candidate_layers{0};
    int heuristic_layers{0};
};

NglEstimationResult estimate_ngl_from_cuda_info(const std::string& model_path,
                                               const std::shared_ptr<spdlog::logger>& logger)
{
    NglEstimationResult result;
    AutoGpuLayerEstimation estimation{};
    std::optional<Utils::CudaMemoryInfo> cuda_info = Utils::query_cuda_memory();

    if (!cuda_info.has_value()) {
        if (logger) {
            logger->warn("Unable to query CUDA memory information, falling back to heuristic");
        }
        return result;
    }

    estimation = estimate_gpu_layers_for_cuda(model_path, *cuda_info);
    result.heuristic_layers = Utils::compute_ngl_from_cuda_memory(*cuda_info);

    int candidate_layers = estimation.layers > 0 ? estimation.layers : 0;
    if (result.heuristic_layers > 0) {
        candidate_layers = std::max(candidate_layers, result.heuristic_layers);
    }
    result.candidate_layers = candidate_layers;

    if (logger) {
        if (estimation.layers > 0 && estimation.layers != candidate_layers) {
            logger->info("CUDA estimator suggested {} layers, but heuristic floor kept {}",
                         estimation.layers, candidate_layers);
        }
        const double to_mib = 1024.0 * 1024.0;
        logger->info(
            "CUDA device total {:.1f} MiB, free {:.1f} MiB -> estimator={}, heuristic={}, chosen={} ({})",
            cuda_info->total_bytes / to_mib,
            cuda_info->free_bytes / to_mib,
            gpu_layers_to_string(estimation.layers),
            gpu_layers_to_string(result.heuristic_layers),
            gpu_layers_to_string(candidate_layers),
            estimation.reason.empty() ? "no estimation detail" : estimation.reason);
    }

    return result;
}

int fallback_ngl(int heuristic_layers, const std::shared_ptr<spdlog::logger>& logger)
{
    if (heuristic_layers > 0) {
        return heuristic_layers;
    }

    const int fallback = Utils::determine_ngl_cuda();
    if (fallback > 0 && logger) {
        logger->info("Using heuristic CUDA fallback -> n_gpu_layers={}",
                     gpu_layers_to_string(fallback));
    }
    return fallback;
}

bool configure_cuda_backend(const std::string& model_path,
                            llama_model_params& params,
                            const std::shared_ptr<spdlog::logger>& logger) {
    if (!ensure_cuda_available(params, logger)) {
        return false;
    }

    const int override_layers = resolve_gpu_layer_override();
    if (apply_ngl_override(override_layers, params, logger)) {
        return true;
    }

    const NglEstimationResult estimation = estimate_ngl_from_cuda_info(model_path, logger);
    int ngl = estimation.candidate_layers;
    if (ngl <= 0) {
        ngl = fallback_ngl(estimation.heuristic_layers, logger);
    }

    if (ngl > 0) {
        params.n_gpu_layers = ngl;
        std::cout << "ngl: " << params.n_gpu_layers << std::endl;
    } else {
        disable_cuda_backend(params, logger, "CUDA not usable after estimation; falling back to CPU.");
        std::cout << "CUDA not usable, falling back to CPU.\n";
    }
    return true;
}
#endif

} // namespace


void silent_logger(enum ggml_log_level, const char *, void *) {}


void llama_debug_logger(enum ggml_log_level level, const char *text, void *user_data) {
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level >= GGML_LOG_LEVEL_ERROR ? spdlog::level::err : spdlog::level::debug,
                    "[llama.cpp] {}", text);
    } else {
        std::fprintf(stderr, "[llama.cpp] %s", text);
    }
}

bool llama_logs_enabled_from_env()
{
    const char* env = std::getenv("AI_FILE_SORTER_LLAMA_LOGS");
    if (!env || env[0] == '\0') {
        env = std::getenv("LLAMA_CPP_DEBUG_LOGS");
    }
    if (!env || env[0] == '\0') {
        return false;
    }

    std::string value{env};
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value != "0" && value != "false" && value != "off" && value != "no";
}


LocalLLMClient::LocalLLMClient(const std::string& model_path,
                               FallbackDecisionCallback fallback_decision_callback)
    : model_path(model_path),
      fallback_decision_callback_(std::move(fallback_decision_callback))
{
    auto logger = Logger::get_logger("core_logger");
    if (logger) {
        logger->info("Initializing local LLM client with model '{}'", model_path);
    }

    configure_llama_logging(logger);
    load_ggml_backends_once(logger);

    const int context_length = std::clamp(resolve_context_length(), 512, 8192);
    llama_model_params model_params = prepare_model_params(logger);

    if (logger) {
        logger->info("Configured context length {} token(s) for local LLM", context_length);
    }

    model_params = load_model_or_throw(model_params, logger);
    configure_context(context_length, model_params);
}


void LocalLLMClient::configure_llama_logging(const std::shared_ptr<spdlog::logger>& logger) const
{
    if (llama_logs_enabled_from_env()) {
        llama_log_set(llama_debug_logger, nullptr);
        if (logger) {
            logger->info("Enabled detailed llama.cpp logging via environment configuration");
        }
    } else {
        llama_log_set(silent_logger, nullptr);
    }
}


llama_model_params build_model_params_for_path(const std::string& model_path,
                                               const std::shared_ptr<spdlog::logger>& logger) {
    load_ggml_backends_once(logger);
    llama_model_params model_params = llama_model_default_params();

#ifdef GGML_USE_METAL
    const char* backend_env = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
    if (backend_env) {
        std::string value(backend_env);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (value == "cpu") {
            if (logger) {
                logger->info("AI_FILE_SORTER_GPU_BACKEND=cpu set; disabling Metal and using CPU backend.");
            }
            model_params.n_gpu_layers = 0;
            return model_params;
        }
    }

    if (!metal_backend_available(logger)) {
        model_params.n_gpu_layers = 0;
        return model_params;
    }

    model_params.n_gpu_layers = determine_metal_layers(model_path, logger);
#else
    const PreferredBackend backend_pref = detect_preferred_backend();
    const char* disable_env = std::getenv("GGML_DISABLE_CUDA");
    const bool cuda_forced_off = disable_env && disable_env[0] != '\0' && disable_env[0] != '0';

    if (apply_cpu_backend(model_params, backend_pref, logger)) {
        return model_params;
    }

    if (backend_pref == PreferredBackend::Vulkan) {
        apply_vulkan_backend(model_path, model_params, logger);
        return model_params;
    }

    if (handle_cuda_forced_off(cuda_forced_off, backend_pref, model_params, logger)) {
        return model_params;
    }

    const bool prefer_vulkan = (backend_pref == PreferredBackend::Vulkan) ||
                               (backend_pref == PreferredBackend::Auto);

    if (prefer_vulkan) {
        // Vulkan is the primary backend; keep CUDA disabled and steer llama.cpp to Vulkan.
        set_env_var("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
        set_env_var("LLAMA_ARG_DEVICE", "vulkan");
        apply_vulkan_backend(model_path, model_params, logger);
        return model_params;
    }

    // CUDA requested explicitly.
    if (handle_cuda_forced_off(cuda_forced_off, backend_pref, model_params, logger)) {
        return model_params;
    }

    const bool cudaConfigured = configure_cuda_backend(model_path, model_params, logger);
    if (!cudaConfigured) {
        if (logger) {
            logger->warn("CUDA backend explicitly requested but unavailable; attempting Vulkan fallback.");
        }
        set_env_var("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
        set_env_var("LLAMA_ARG_DEVICE", "vulkan");
        apply_vulkan_backend(model_path, model_params, logger);
        return model_params;
    }
#endif

    return model_params;
}

llama_model_params LocalLLMClient::prepare_model_params(const std::shared_ptr<spdlog::logger>& logger)
{
    return build_model_params_for_path(model_path, logger);
}

#if defined(AI_FILE_SORTER_TEST_BUILD) && !defined(GGML_USE_METAL)
namespace {

PreferredBackend to_internal_backend(LocalLLMTestAccess::BackendPreference preference) {
    switch (preference) {
        case LocalLLMTestAccess::BackendPreference::Cpu: return PreferredBackend::Cpu;
        case LocalLLMTestAccess::BackendPreference::Cuda: return PreferredBackend::Cuda;
        case LocalLLMTestAccess::BackendPreference::Vulkan: return PreferredBackend::Vulkan;
        case LocalLLMTestAccess::BackendPreference::Auto:
        default:
            return PreferredBackend::Auto;
    }
}

LocalLLMTestAccess::BackendPreference to_external_backend(PreferredBackend preference) {
    switch (preference) {
        case PreferredBackend::Cpu: return LocalLLMTestAccess::BackendPreference::Cpu;
        case PreferredBackend::Cuda: return LocalLLMTestAccess::BackendPreference::Cuda;
        case PreferredBackend::Vulkan: return LocalLLMTestAccess::BackendPreference::Vulkan;
        case PreferredBackend::Auto:
        default:
            return LocalLLMTestAccess::BackendPreference::Auto;
    }
}

} // namespace

namespace LocalLLMTestAccess {

BackendPreference detect_preferred_backend() {
    return to_external_backend(::detect_preferred_backend());
}

bool apply_cpu_backend(llama_model_params& params, BackendPreference preference) {
    return ::apply_cpu_backend(params, to_internal_backend(preference), nullptr);
}

bool apply_vulkan_backend(const std::string& model_path, llama_model_params& params) {
    return ::apply_vulkan_backend(model_path, params, nullptr);
}

bool handle_cuda_forced_off(bool cuda_forced_off,
                            BackendPreference preference,
                            llama_model_params& params) {
    return ::handle_cuda_forced_off(cuda_forced_off, to_internal_backend(preference), params, nullptr);
}

bool configure_cuda_backend(const std::string& model_path, llama_model_params& params) {
    return ::configure_cuda_backend(model_path, params, nullptr);
}

llama_model_params prepare_model_params_for_testing(const std::string& model_path) {
    return ::build_model_params_for_path(model_path, nullptr);
}

} // namespace LocalLLMTestAccess
#endif // AI_FILE_SORTER_TEST_BUILD && !GGML_USE_METAL


llama_model_params LocalLLMClient::load_model_or_throw(llama_model_params model_params,
                                                       const std::shared_ptr<spdlog::logger>& logger)
{
    auto try_load = [&](const llama_model_params& params) {
        model = llama_model_load_from_file(model_path.c_str(), params);
        if (!model) {
            return false;
        }
        if (logger) {
            logger->info("Loaded local model '{}'", model_path);
        }
        vocab = llama_model_get_vocab(model);
        return true;
    };

    if (try_load(model_params)) {
        return model_params;
    }

    if (model_params.n_gpu_layers != 0) {
        if (logger) {
            logger->warn("Failed to load model with GPU backend; retrying on CPU.");
        }
        if (!allow_gpu_fallback(fallback_decision_callback_, logger, "model load failure")) {
            if (logger) {
                logger->warn("GPU fallback declined during model load; aborting.");
            }
            throw std::runtime_error("GPU backend failed to initialize and CPU fallback was declined.");
        }
        notify_status(Status::GpuFallbackToCpu);
        set_env_var("AI_FILE_SORTER_GPU_BACKEND", "cpu");
        set_env_var("LLAMA_ARG_DEVICE", "cpu");
        model_params.n_gpu_layers = 0;
        if (try_load(model_params)) {
            return model_params;
        }
    }

    if (logger) {
        logger->error("Failed to load model from '{}'", model_path);
    }
    throw std::runtime_error("Failed to load model");
}


void LocalLLMClient::configure_context(int context_length, const llama_model_params& model_params)
{
    ctx_params = llama_context_default_params();
    ctx_params.n_ctx = context_length;
    ctx_params.n_batch = context_length;
#ifdef GGML_USE_METAL
    if (model_params.n_gpu_layers != 0) {
        ctx_params.offload_kqv = true;
    }
#else
    (void)model_params;
#endif
}


std::string LocalLLMClient::make_prompt(const std::string& file_name,
                                        const std::string& file_path,
                                        FileType file_type,
                                        const std::string& consistency_context)
{
    std::ostringstream user_section;
    if (!file_path.empty()) {
        user_section << "\nFull path: " << file_path << "\n";
    }
    user_section << "Name: " << file_name << "\n";

    std::string prompt = (file_type == FileType::File)
        ? "\nCategorize this file:\n" + user_section.str()
        : "\nCategorize the directory:\n" + user_section.str();

    if (!consistency_context.empty()) {
        prompt += "\n" + consistency_context + "\n";
    }

    std::string instruction = R"(<|begin_of_text|><|start_header_id|>system<|end_header_id|>
    You are a file categorization assistant. You must always follow the exact format. If the file is an installer, determine the type of software it installs. Base your answer on the filename, extension, and any directory context provided. The output must be:
    <Main category> : <Subcategory>
    Main category must be broad (one or two words, plural). Subcategory must be specific, relevant, and never just repeat the main category. Output exactly one line. Do not explain, add line breaks, or use words like 'Subcategory'. If uncertain, always make your best guess based on the name only. Do not apologize or state uncertainty. Never say you lack information.
    Examples:
    Texts : Documents
    Productivity : File managers
    Tables : Financial logs
    Utilities : Task managers
    <|eot_id|><|start_header_id|>user<|end_header_id|>
    )" + prompt + R"(<|eot_id|><|start_header_id|>assistant<|end_header_id|>)";

    return instruction;
}


std::string LocalLLMClient::generate_response(const std::string &prompt,
                                              int n_predict,
                                              bool apply_sanitizer)
{
    auto logger = Logger::get_logger("core_logger");
    if (logger) {
        logger->debug("Generating response with prompt length {} tokens target {}", prompt.size(), n_predict);
    }

    struct ContextAttempt {
        int n_ctx;
        int n_batch;
    };

    auto build_context_attempts = [](int n_ctx, int n_batch) {
        std::vector<ContextAttempt> attempts;
        auto add_attempt = [&](int ctx, int batch) {
            ctx = std::max(ctx, 512);
            batch = std::clamp(batch, 1, ctx);
            if (ctx > n_ctx || batch > n_batch) {
                return;
            }
            if (ctx == n_ctx && batch == n_batch) {
                return;
            }
            for (const auto& existing : attempts) {
                if (existing.n_ctx == ctx && existing.n_batch == batch) {
                    return;
                }
            }
            attempts.push_back({ctx, batch});
        };

        add_attempt(std::min(n_ctx, 2048), std::min(n_batch, 1024));
        add_attempt(std::min(n_ctx, 1024), std::min(n_batch, 512));
        add_attempt(std::min(n_ctx, 512), std::min(n_batch, 256));
        return attempts;
    };

    auto try_init_context = [&](const llama_context_params& base_params,
                                int n_ctx,
                                int n_batch,
                                llama_context_params& resolved_params) -> llama_context* {
        llama_context_params attempt = base_params;
        attempt.n_ctx = n_ctx;
        attempt.n_batch = std::min(n_batch, n_ctx);
        auto* ctx = llama_init_from_model(model, attempt);
        if (ctx) {
            resolved_params = attempt;
        }
        return ctx;
    };

    auto init_context_with_retries = [&](const llama_context_params& base_params,
                                         bool cpu_attempt,
                                         llama_context_params& resolved_params) -> llama_context* {
        auto* ctx = try_init_context(base_params, base_params.n_ctx, base_params.n_batch, resolved_params);
        if (ctx) {
            return ctx;
        }
        if (logger) {
            logger->warn("Failed to initialize llama context (n_ctx={}, n_batch={}); retrying with smaller buffers{}",
                         base_params.n_ctx,
                         base_params.n_batch,
                         cpu_attempt ? " on CPU" : "");
        }
        for (const auto& attempt : build_context_attempts(base_params.n_ctx, base_params.n_batch)) {
            if (logger) {
                logger->warn("Retrying llama context init with n_ctx={}, n_batch={}{}",
                             attempt.n_ctx,
                             attempt.n_batch,
                             cpu_attempt ? " on CPU" : "");
            }
            ctx = try_init_context(base_params, attempt.n_ctx, attempt.n_batch, resolved_params);
            if (ctx) {
                return ctx;
            }
        }
        return nullptr;
    };

    bool allow_fallback = true;
    for (;;) {
        llama_context* ctx = nullptr;
        llama_sampler* smpl = nullptr;
        try {
            llama_context_params resolved_params = ctx_params;
            llama_context_params base_params = ctx_params;
            ctx = init_context_with_retries(base_params, false, resolved_params);

            if (!ctx && !is_cpu_backend_requested()) {
                if (!allow_gpu_fallback(fallback_decision_callback_, logger, "context initialization failure")) {
                    allow_fallback = false;
                    throw std::runtime_error("GPU backend failed during context initialization and CPU fallback was declined.");
                }
                if (logger) {
                    logger->warn("Context init failed on GPU; reloading model on CPU and retrying.");
                }
                notify_status(Status::GpuFallbackToCpu);
                llama_model_params cpu_params = llama_model_default_params();
                cpu_params.n_gpu_layers = 0;
                set_env_var("AI_FILE_SORTER_GPU_BACKEND", "cpu");
                set_env_var("LLAMA_ARG_DEVICE", "cpu");
                set_env_var("GGML_DISABLE_CUDA", "1");

                llama_model* old_model = model;
                llama_model* cpu_model = llama_model_load_from_file(model_path.c_str(), cpu_params);
                if (!cpu_model) {
                    if (logger) {
                        logger->error("Failed to reload model on CPU after context init failure");
                    }
                } else {
                    if (old_model) {
                        llama_model_free(old_model);
                    }
                    model = cpu_model;
                    vocab = llama_model_get_vocab(model);
#ifdef GGML_USE_METAL
                    base_params = ctx_params;
                    base_params.offload_kqv = false;
#else
                    base_params = ctx_params;
#endif
                    resolved_params = base_params;
                    ctx = init_context_with_retries(base_params, true, resolved_params);
                }
            }

            if (!ctx) {
                if (logger) {
                    logger->error("Failed to initialize llama context");
                }
                return "";
            }

            ctx_params = resolved_params;

            smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
            llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f, 1));
            llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f));
            llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

            std::string final_prompt;
            if (!format_prompt(model, prompt, final_prompt)) {
                if (logger) {
                    logger->error("Failed to apply chat template to prompt");
                }
                llama_free(ctx);
                llama_sampler_free(smpl);
                return "";
            }

            std::vector<llama_token> prompt_tokens;
            int n_prompt = 0;
            if (!tokenize_prompt(vocab, final_prompt, prompt_tokens, n_prompt, logger)) {
                llama_free(ctx);
                llama_sampler_free(smpl);
                return "";
            }

            std::string output = run_generation_loop(ctx,
                                                     smpl,
                                                     prompt_tokens,
                                                     n_prompt,
                                                     n_predict,
                                                     logger,
                                                     vocab);

            llama_sampler_reset(smpl);
            llama_free(ctx);
            llama_sampler_free(smpl);

            if (logger) {
                logger->debug("Generation complete, produced {} character(s)", output.size());
            }

            if (apply_sanitizer) {
                return sanitize_output(output);
            }
            return output;
        } catch (const std::exception& ex) {
            if (ctx) {
                llama_free(ctx);
            }
            if (smpl) {
                llama_sampler_free(smpl);
            }

            if (allow_fallback && !is_cpu_backend_requested()) {
                if (!allow_gpu_fallback(fallback_decision_callback_, logger, "generation failure")) {
                    allow_fallback = false;
                    throw std::runtime_error("GPU backend failed during generation and CPU fallback was declined.");
                }
                allow_fallback = false;
                if (logger) {
                    logger->warn("LLM generation failed on GPU ({}); retrying on CPU.", ex.what());
                }
                notify_status(Status::GpuFallbackToCpu);

                llama_model_params cpu_params = llama_model_default_params();
                cpu_params.n_gpu_layers = 0;
                set_env_var("AI_FILE_SORTER_GPU_BACKEND", "cpu");
                set_env_var("LLAMA_ARG_DEVICE", "cpu");
                set_env_var("GGML_DISABLE_CUDA", "1");

                llama_model* old_model = model;
                llama_model* cpu_model = llama_model_load_from_file(model_path.c_str(), cpu_params);
                if (!cpu_model) {
                    if (logger) {
                        logger->error("Failed to reload model on CPU after GPU error");
                    }
                } else {
                    if (old_model) {
                        llama_model_free(old_model);
                    }
                    model = cpu_model;
                    vocab = llama_model_get_vocab(model);
#ifdef GGML_USE_METAL
                    ctx_params.offload_kqv = false;
#endif
                    continue;
                }
            }

            if (logger) {
                logger->error("LLM generation failed: {}", ex.what());
            }
            throw;
        }
    }
}


std::string LocalLLMClient::categorize_file(const std::string& file_name,
                                            const std::string& file_path,
                                            FileType file_type,
                                            const std::string& consistency_context)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        if (!file_path.empty()) {
            logger->debug("Requesting local categorization for '{}' ({}) at '{}'",
                          file_name, to_string(file_type), file_path);
        } else {
            logger->debug("Requesting local categorization for '{}' ({})", file_name, to_string(file_type));
        }
    }
    std::string prompt = make_prompt(file_name, file_path, file_type, consistency_context);
    if (prompt_logging_enabled) {
        std::cout << "\n[DEV][PROMPT] Categorization request\n" << prompt << "\n";
    }
    std::string response = generate_response(prompt, 64, true);
    if (prompt_logging_enabled) {
        std::cout << "[DEV][RESPONSE] Categorization reply\n" << response << "\n";
    }
    return response;
}


std::string LocalLLMClient::complete_prompt(const std::string& prompt,
                                            int max_tokens)
{
    const int capped = max_tokens > 0 ? max_tokens : 256;
    return generate_response(prompt, capped, false);
}


std::string LocalLLMClient::sanitize_output(std::string& output) {
    output.erase(0, output.find_first_not_of(" \t\n\r\f\v"));
    output.erase(output.find_last_not_of(" \t\n\r\f\v") + 1);

    std::regex pattern(R"(([^:\s][^\n:]*?\s*:\s*[^\n]+))");
    std::smatch match;
    if (std::regex_search(output, match, pattern)) {
    std::string result = match[1];

    result.erase(0, result.find_first_not_of(" \t\n\r\f\v"));
    result.erase(result.find_last_not_of(" \t\n\r\f\v") + 1);

    size_t paren_pos = result.find(" (");
    if (paren_pos != std::string::npos) {
        result.erase(paren_pos);
        result.erase(result.find_last_not_of(" \t\n\r\f\v") + 1);
    }

    return result;
}

    return output;
}



LocalLLMClient::~LocalLLMClient() {
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->debug("Destroying LocalLLMClient for model '{}'", model_path);
    }
    if (model) llama_model_free(model);
}

void LocalLLMClient::set_prompt_logging_enabled(bool enabled)
{
    prompt_logging_enabled = enabled;
}

void LocalLLMClient::set_status_callback(StatusCallback callback)
{
    status_callback_ = std::move(callback);
}

void LocalLLMClient::set_fallback_decision_callback(FallbackDecisionCallback callback)
{
    fallback_decision_callback_ = std::move(callback);
}

void LocalLLMClient::notify_status(Status status) const
{
    if (status_callback_) {
        status_callback_(status);
    }
}
