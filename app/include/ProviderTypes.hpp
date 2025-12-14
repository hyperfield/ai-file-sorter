#pragma once
#include <string>
#include <vector>
#include <optional>

enum class ProviderKind {
    LocalOllama,
    OllamaCloud,
    OpenAI,
    LocalGGUF
};

struct OllamaModel {
    std::string name;
    std::string digest;
    std::string family;
    std::string parameter_size;
    std::string quantization_level;
    uint64_t size_bytes = 0;

    std::string display_name() const {
        if (!parameter_size.empty() && !quantization_level.empty()) {
            return name + " (" + parameter_size + ", " + quantization_level + ")";
        }
        return name;
    }
};

struct HealthResult {
    bool ok = false;
    std::string message;
    int http_code = 0;

    static HealthResult success() {
        return {true, "OK", 200};
    }

    static HealthResult error(const std::string& msg, int code = 0) {
        return {false, msg, code};
    }
};

struct ListModelsResult {
    bool ok = false;
    std::string error_message;
    std::vector<OllamaModel> models;

    static ListModelsResult success(std::vector<OllamaModel> models) {
        ListModelsResult r;
        r.ok = true;
        r.models = std::move(models);
        return r;
    }

    static ListModelsResult error(const std::string& msg) {
        ListModelsResult r;
        r.ok = false;
        r.error_message = msg;
        return r;
    }
};
