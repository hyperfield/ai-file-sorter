/*
 * Provider Smoke Test CLI
 * 
 * A headless CLI tool to test Ollama provider functionality without the Qt GUI.
 * 
 * Usage:
 *   provider_smoke --provider local --list-models
 *   provider_smoke --provider local --health
 *   provider_smoke --provider cloud --base-url https://example.com --list-models
 *   provider_smoke --provider cloud --base-url https://example.com --health
 * 
 * Environment Variables:
 *   OLLAMA_API_KEY - Required for cloud provider
 *   OLLAMA_BASE_URL - Optional override for local provider base URL
 *   OLLAMA_CLOUD_URL - Optional override for cloud provider base URL
 */

#include "LocalOllamaProvider.hpp"
#include "OllamaCloudProvider.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <memory>
#include <cstdlib>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --provider <local|cloud>  Provider type (required)\n"
              << "  --base-url <url>          Base URL override\n"
              << "  --list-models             List available models\n"
              << "  --health                  Check provider health\n"
              << "  --help                    Show this help message\n\n"
              << "Environment Variables:\n"
              << "  OLLAMA_API_KEY            API key for cloud provider\n"
              << "  OLLAMA_BASE_URL           Base URL override for local provider\n"
              << "  OLLAMA_CLOUD_URL          Base URL for cloud provider\n";
}

std::string get_env_or(const char* name, const std::string& fallback) {
    const char* val = std::getenv(name);
    return (val && val[0] != '\0') ? std::string(val) : fallback;
}

int main(int argc, char* argv[]) {
    std::string provider_type;
    std::string base_url;
    bool list_models = false;
    bool check_health = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "--provider") == 0 && i + 1 < argc) {
            provider_type = argv[++i];
        } else if (std::strcmp(argv[i], "--base-url") == 0 && i + 1 < argc) {
            base_url = argv[++i];
        } else if (std::strcmp(argv[i], "--list-models") == 0) {
            list_models = true;
        } else if (std::strcmp(argv[i], "--health") == 0) {
            check_health = true;
        }
    }

    if (provider_type.empty()) {
        std::cerr << "Error: --provider is required\n\n";
        print_usage(argv[0]);
        return 1;
    }

    if (!list_models && !check_health) {
        std::cerr << "Error: specify --list-models or --health\n\n";
        print_usage(argv[0]);
        return 1;
    }

    std::unique_ptr<IProvider> provider;

    if (provider_type == "local") {
        if (base_url.empty()) {
            base_url = get_env_or("OLLAMA_BASE_URL", "http://localhost:11434");
        }
        std::cout << "Provider: Local Ollama\n";
        std::cout << "Base URL: " << base_url << "\n\n";
        provider = std::make_unique<LocalOllamaProvider>(base_url);
    } else if (provider_type == "cloud") {
        if (base_url.empty()) {
            base_url = get_env_or("OLLAMA_CLOUD_URL", "");
        }
        if (base_url.empty()) {
            std::cerr << "Error: Cloud provider requires --base-url or OLLAMA_CLOUD_URL env var\n";
            return 1;
        }
        std::cout << "Provider: Ollama Cloud\n";
        std::cout << "Base URL: " << base_url << "\n";
        
        auto cloud = std::make_unique<OllamaCloudProvider>(base_url);
        if (!cloud->has_api_key()) {
            std::cerr << "Warning: OLLAMA_API_KEY not set\n";
        } else {
            std::cout << "API Key: (set)\n";
        }
        std::cout << "\n";
        provider = std::move(cloud);
    } else {
        std::cerr << "Error: Unknown provider type '" << provider_type << "'\n";
        std::cerr << "Valid options: local, cloud\n";
        return 1;
    }

    if (check_health) {
        std::cout << "Checking health...\n";
        HealthResult health = provider->check_health();
        if (health.ok) {
            std::cout << "Status: OK\n";
        } else {
            std::cout << "Status: FAILED\n";
            std::cout << "Error: " << health.message << "\n";
            if (health.http_code > 0) {
                std::cout << "HTTP Code: " << health.http_code << "\n";
            }
            return 1;
        }
    }

    if (list_models) {
        std::cout << "Fetching models...\n";
        ListModelsResult result = provider->list_models();
        if (!result.ok) {
            std::cout << "Error: " << result.error_message << "\n";
            return 1;
        }

        std::cout << "\nModels (" << result.models.size() << "):\n";
        std::cout << std::string(60, '-') << "\n";
        
        for (const auto& model : result.models) {
            std::cout << "  " << model.display_name() << "\n";
            if (!model.family.empty()) {
                std::cout << "    Family: " << model.family << "\n";
            }
            if (model.size_bytes > 0) {
                double size_gb = static_cast<double>(model.size_bytes) / (1024.0 * 1024.0 * 1024.0);
                std::cout << "    Size: " << std::fixed << size_gb << " GB\n";
            }
        }
        
        if (result.models.empty()) {
            std::cout << "  (no models found)\n";
        }
    }

    std::cout << "\nDone.\n";
    return 0;
}
