#include "LocalLLMClient.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "llama.h"
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
    return 2048;
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
#endif // defined(GGML_USE_METAL)

std::optional<int32_t> extract_block_count(const std::string & model_path) {
    std::ifstream file(model_path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    constexpr std::size_t kScanBytes = 8 * 1024 * 1024; // first 8 MiB should contain metadata
    std::vector<char> buffer(kScanBytes);
    const std::streamsize to_read = static_cast<std::streamsize>(buffer.size());
    file.read(buffer.data(), to_read);
    if (file.bad()) {
        return std::nullopt;
    }

    const std::size_t bytes_read = static_cast<std::size_t>(file.gcount());
    if (bytes_read == 0) {
        return std::nullopt;
    }

    const std::string_view data(buffer.data(), bytes_read);
    static const std::string_view candidate_keys[] = {
        "llama.block_count",
        "llama.layer_count",
        "llama.n_layer",
    };

    auto read_le32 = [](const char * ptr) -> uint32_t {
        uint32_t value;
        std::memcpy(&value, ptr, sizeof(uint32_t));
        return value;
    };

    auto read_le64 = [](const char * ptr) -> uint64_t {
        uint64_t value;
        std::memcpy(&value, ptr, sizeof(uint64_t));
        return value;
    };

    for (const auto & key : candidate_keys) {
        std::size_t pos = data.find(key);
        while (pos != std::string_view::npos) {
            if (pos < sizeof(uint64_t)) {
                pos = data.find(key, pos + 1);
                continue;
            }

            const uint64_t declared_len = read_le64(buffer.data() + pos - sizeof(uint64_t));
            if (declared_len != key.size()) {
                pos = data.find(key, pos + 1);
                continue;
            }

            const std::size_t type_offset = pos + key.size();
            if (type_offset + sizeof(uint32_t) > bytes_read) {
                break;
            }

            const uint32_t type = read_le32(buffer.data() + type_offset);
            const std::size_t value_offset = type_offset + sizeof(uint32_t);

            int32_t parsed_value = 0;
            bool parsed = false;

            switch (type) {
                case 4: // GGUF_TYPE_UINT32
                    if (value_offset + sizeof(uint32_t) <= bytes_read) {
                        parsed_value = static_cast<int32_t>(read_le32(buffer.data() + value_offset));
                        parsed = true;
                    }
                    break;
                case 5: // GGUF_TYPE_INT32
                    if (value_offset + sizeof(uint32_t) <= bytes_read) {
                        parsed_value = static_cast<int32_t>(read_le32(buffer.data() + value_offset));
                        parsed = true;
                    }
                    break;
                case 10: // GGUF_TYPE_UINT64
                    if (value_offset + sizeof(uint64_t) <= bytes_read) {
                        parsed_value = static_cast<int32_t>(read_le64(buffer.data() + value_offset));
                        parsed = true;
                    }
                    break;
                case 11: // GGUF_TYPE_INT64
                    if (value_offset + sizeof(uint64_t) <= bytes_read) {
                        parsed_value = static_cast<int32_t>(read_le64(buffer.data() + value_offset));
                        parsed = true;
                    }
                    break;
                default:
                    break;
            }

            if (parsed) {
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

[[maybe_unused]] AutoGpuLayerEstimation estimate_gpu_layers_for_cuda(const std::string & model_path,
                                                                     const Utils::CudaMemoryInfo & memory_info) {
    AutoGpuLayerEstimation result;

    if (!memory_info.valid()) {
        result.layers = -1;
        result.reason = "CUDA memory metrics unavailable";
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
    const double bytes_per_layer =
        static_cast<double>(file_size) / static_cast<double>(total_layers);

    double approx_free = static_cast<double>(memory_info.free_bytes);
    double total_bytes = static_cast<double>(memory_info.total_bytes);

    if (total_bytes <= 0.0) {
        total_bytes = approx_free;
    }

    const double usable_total = std::max(total_bytes, approx_free);
    if (usable_total <= 0.0) {
        result.layers = 0;
        result.reason = "CUDA memory metrics invalid";
        return result;
    }

    if (approx_free <= 0.0) {
        approx_free = usable_total * 0.80;
    } else if (approx_free > usable_total) {
        approx_free = usable_total;
    }

    if (approx_free <= 0.0 || bytes_per_layer <= 0.0) {
        result.layers = 0;
        result.reason = "insufficient CUDA memory metrics";
        return result;
    }

    const double safety_reserve =
        std::max(usable_total * 0.05, 192.0 * 1024.0 * 1024.0); // keep at least 5% or 192 MiB free
    double budget_bytes = approx_free - safety_reserve;
    if (budget_bytes <= 0.0) {
        budget_bytes = approx_free * 0.75;
    }

    const double max_budget = std::min(approx_free * 0.98, usable_total * 0.90);
    const double min_budget = usable_total * 0.45;
    budget_bytes = std::clamp(budget_bytes, min_budget, max_budget);

    constexpr double overhead_factor = 1.08;
    int32_t estimated_layers =
        static_cast<int32_t>(std::floor(budget_bytes / (bytes_per_layer * overhead_factor)));

    if (estimated_layers <= 0) {
        result.layers = 0;
        result.reason = "insufficient CUDA memory budget";
        return result;
    }

    estimated_layers = std::clamp<int32_t>(estimated_layers, 1, total_layers);

    result.layers = estimated_layers;
    result.reason = "estimated from CUDA memory headroom";
    return result;
}

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


LocalLLMClient::LocalLLMClient(const std::string& model_path)
    : model_path(model_path)
{
    auto logger = Logger::get_logger("core_logger");
    if (logger) {
        logger->info("Initializing local LLM client with model '{}'", model_path);
    }

    configure_llama_logging(logger);

    const int context_length = std::clamp(resolve_context_length(), 512, 8192);
    llama_model_params model_params = prepare_model_params(logger);

    ggml_backend_load_all();

    if (logger) {
        logger->info("Configured context length {} token(s) for local LLM", context_length);
    }

    load_model_or_throw(model_params, logger);
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


llama_model_params LocalLLMClient::prepare_model_params(const std::shared_ptr<spdlog::logger>& logger)
{
    llama_model_params model_params = llama_model_default_params();

#ifdef GGML_USE_METAL
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

    model_params.n_gpu_layers = gpu_layers;
#else
    const bool cuda_available = Utils::is_cuda_available();
    if (!cuda_available) {
        set_env_var("GGML_DISABLE_CUDA", "1");
    }

    if (cuda_available) {
        const int override_layers = resolve_gpu_layer_override();
        if (override_layers != INT_MIN) {
            if (override_layers <= 0) {
                model_params.n_gpu_layers = 0;
                set_env_var("GGML_DISABLE_CUDA", "1");
                if (logger) {
                    logger->info("CUDA disabled via override (AI_FILE_SORTER_N_GPU_LAYERS={})",
                                 override_layers);
                }
            } else {
                model_params.n_gpu_layers = override_layers;
                if (logger) {
                    logger->info("Using explicit CUDA n_gpu_layers override {}",
                                 gpu_layers_to_string(override_layers));
                }
                std::cout << "ngl override: " << model_params.n_gpu_layers << std::endl;
            }
        } else {
            int ngl = 0;
            int heuristic_from_info = 0;
            AutoGpuLayerEstimation estimation{};
            std::optional<Utils::CudaMemoryInfo> cuda_info = Utils::query_cuda_memory();

            if (cuda_info.has_value()) {
                estimation = estimate_gpu_layers_for_cuda(model_path, *cuda_info);
                heuristic_from_info = Utils::compute_ngl_from_cuda_memory(*cuda_info);

                int candidate_layers = estimation.layers > 0 ? estimation.layers : 0;
                if (heuristic_from_info > 0) {
                    candidate_layers = std::max(candidate_layers, heuristic_from_info);
                }
                ngl = candidate_layers;

                if (estimation.layers > 0 && logger && estimation.layers != candidate_layers) {
                    logger->info("CUDA estimator suggested {} layers, but heuristic floor kept {}",
                                 estimation.layers, candidate_layers);
                }

                if (logger) {
                    const double to_mib = 1024.0 * 1024.0;
                    logger->info(
                        "CUDA device total {:.1f} MiB, free {:.1f} MiB -> estimator={}, heuristic={}, chosen={} ({})",
                        cuda_info->total_bytes / to_mib,
                        cuda_info->free_bytes / to_mib,
                        gpu_layers_to_string(estimation.layers),
                        gpu_layers_to_string(heuristic_from_info),
                        gpu_layers_to_string(ngl),
                        estimation.reason.empty() ? "no estimation detail" : estimation.reason
                    );
                }
            } else if (logger) {
                logger->warn("Unable to query CUDA memory information, falling back to heuristic");
            }

            if (ngl <= 0) {
                ngl = (heuristic_from_info > 0)
                    ? heuristic_from_info
                    : Utils::determine_ngl_cuda();
                if (logger && ngl > 0) {
                    logger->info("Using heuristic CUDA fallback -> n_gpu_layers={}",
                                 gpu_layers_to_string(ngl));
                }
            }

            if (ngl > 0) {
                model_params.n_gpu_layers = ngl;
                std::cout << "ngl: " << model_params.n_gpu_layers << std::endl;
            } else {
                model_params.n_gpu_layers = 0;
                set_env_var("GGML_DISABLE_CUDA", "1");
                std::cout << "CUDA not usable, falling back to CPU.\n";
            }
        }
    } else {
        model_params.n_gpu_layers = 0;
        printf("model_params.n_gpu_layers: %d\n", model_params.n_gpu_layers);
        if (logger) {
            logger->info("CUDA backend unavailable; using CPU backend");
        }
        std::cout << "No supported GPU backend detected. Running on CPU.\n";
    }
#endif

    return model_params;
}


void LocalLLMClient::load_model_or_throw(const llama_model_params& model_params,
                                         const std::shared_ptr<spdlog::logger>& logger)
{
    model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model) {
        if (logger) {
            logger->error("Failed to load model from '{}'", model_path);
        }
        throw std::runtime_error("Failed to load model");
    }

    if (logger) {
        logger->info("Loaded local model '{}'", model_path);
    }

    vocab = llama_model_get_vocab(model);
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
                                        FileType file_type)
{
    std::ostringstream user_section;
    if (!file_path.empty()) {
        user_section << "\nFull path: " << file_path << "\n";
    }
    user_section << "Name: " << file_name << "\n";

    std::string prompt = (file_type == FileType::File)
        ? "\nCategorize this file:\n" + user_section.str()
        : "\nCategorize the directory:\n" + user_section.str();

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
                                              int n_predict)
{
    auto logger = Logger::get_logger("core_logger");
    if (logger) {
        logger->debug("Generating response with prompt length {} tokens target {}", prompt.size(), n_predict);
    }

    auto* ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
        if (logger) {
            logger->error("Failed to initialize llama context");
        }
        return "";
    }

    auto* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    std::vector<llama_chat_message> messages;
    messages.push_back({"user", prompt.c_str()});
    const char * tmpl = llama_model_chat_template(model, nullptr);
    std::vector<char> formatted_prompt(8192);

    int actual_len = llama_chat_apply_template(tmpl, messages.data(),
                                               messages.size(), true,
                                               formatted_prompt.data(),
                                               formatted_prompt.size());
    if (actual_len < 0) {
        if (logger) {
            logger->error("Failed to apply chat template to prompt");
        }
        fprintf(stderr, "Failed to apply chat template\n");
        return "";
    }
    std::string final_prompt(formatted_prompt.data(), actual_len);

    const int n_prompt = -llama_tokenize(vocab, final_prompt.c_str(),
                                         final_prompt.size(),
                                         NULL, 0, true, true);
    std::vector<llama_token> prompt_tokens(n_prompt);

    if (llama_tokenize(vocab, final_prompt.c_str(), final_prompt.size(),
                       prompt_tokens.data(), prompt_tokens.size(), true,
                       true) < 0) {
        fprintf(stderr, "%s: error: failed to tokenize the prompt\n", __func__);
        if (logger) {
            logger->error("Tokenization failed for prompt");
        }
        llama_model_free(model);
        return "";
    }

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(),
                                            prompt_tokens.size());
    llama_token new_token_id;
    std::string output;

    const int max_tokens = n_predict;
    int generated_tokens = 0;
    for (int n_pos = 0; generated_tokens < max_tokens; ) {
        if (llama_decode(ctx, batch)) {
            if (logger) {
                logger->warn("llama_decode returned non-zero status; aborting generation");
            }
            break;
        }

        n_pos += batch.n_tokens;
        new_token_id = llama_sampler_sample(smpl, ctx, -1);

        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }

        if (n_pos >= n_prompt) {
            char buf[128];
            int n = llama_token_to_piece(vocab, new_token_id, buf,
                                         sizeof(buf), 0, true);
            if (n < 0) break;
            output.append(buf, n);
            generated_tokens++;
        }

        batch = llama_batch_get_one(&new_token_id, 1);
    }

    while (!output.empty() && std::isspace(output.front())) {
        output.erase(output.begin());
    }

    llama_sampler_reset(smpl);
    llama_free(ctx);

    if (logger) {
        logger->debug("Generation complete, produced {} character(s)", output.size());
    }

    return sanitize_output(output);
}


std::string LocalLLMClient::categorize_file(const std::string& file_name,
                                            const std::string& file_path,
                                            FileType file_type)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        if (!file_path.empty()) {
            logger->debug("Requesting local categorization for '{}' ({}) at '{}'",
                          file_name, to_string(file_type), file_path);
        } else {
            logger->debug("Requesting local categorization for '{}' ({})", file_name, to_string(file_type));
        }
    }
    std::string prompt = make_prompt(file_name, file_path, file_type);
    return generate_response(prompt, 64);
}


std::string LocalLLMClient::complete_prompt(const std::string& prompt,
                                            int max_tokens)
{
    const int capped = max_tokens > 0 ? max_tokens : 256;
    return generate_response(prompt, capped);
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
