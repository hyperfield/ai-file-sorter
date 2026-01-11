#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>
#include <QApplication>
#include "TranslationManager.hpp"
#include "Language.hpp"

inline std::string make_unique_token(std::string_view prefix) {
    static std::atomic<uint64_t> counter{0};
    const uint64_t value = counter.fetch_add(1, std::memory_order_relaxed);
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::string(prefix) + std::to_string(now) + "-" + std::to_string(value);
}

class EnvVarGuard {
public:
    EnvVarGuard(std::string key, std::optional<std::string> value)
        : key_(std::move(key)) {
        if (const char* existing = std::getenv(key_.c_str())) {
            original_ = existing;
        }
        apply(value);
    }

    ~EnvVarGuard() {
        apply(original_);
    }

    EnvVarGuard(const EnvVarGuard&) = delete;
    EnvVarGuard& operator=(const EnvVarGuard&) = delete;

private:
    static void set_env(const std::string& key, const std::string& value) {
#ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }

    static void unset_env(const std::string& key) {
#ifdef _WIN32
        _putenv_s(key.c_str(), "");
#else
        unsetenv(key.c_str());
#endif
    }

    void apply(const std::optional<std::string>& value) {
        if (value.has_value()) {
            set_env(key_, *value);
        } else {
            unset_env(key_);
        }
    }

    std::string key_;
    std::optional<std::string> original_;
};

class TempDir {
public:
    TempDir()
        : path_(std::filesystem::temp_directory_path() /
                make_unique_token("aifs-test-")) {
        std::filesystem::create_directories(path_);
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

class TempModelFile {
public:
    explicit TempModelFile(std::uint32_t block_count = 32,
                           std::size_t file_size = 4 * 1024 * 1024) {
        if (file_size < 32) {
            file_size = 32;
        }
        path_ = std::filesystem::temp_directory_path() /
                (make_unique_token("aifs-model-") + ".gguf");
        std::vector<char> buffer(file_size, 0);
        const std::string key = "llama.block_count";
        const std::uint64_t len = static_cast<std::uint64_t>(key.size());
        const std::uint32_t type = 4; // GGUF_TYPE_UINT32
        const std::uint32_t value = block_count;

        const std::size_t required =
            sizeof(len) + key.size() + sizeof(type) + sizeof(value);
        if (buffer.size() < required) {
            buffer.resize(required);
        }

        std::size_t offset = 0;
        std::memcpy(&buffer[offset], &len, sizeof(len));
        offset += sizeof(len);
        std::memcpy(&buffer[offset], key.data(), key.size());
        offset += key.size();
        std::memcpy(&buffer[offset], &type, sizeof(type));
        offset += sizeof(type);
        std::memcpy(&buffer[offset], &value, sizeof(value));

        std::ofstream out(path_, std::ios::binary);
        out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        out.close();
    }

    ~TempModelFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

class QtAppContext {
public:
    QtAppContext() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "tests";
            static char* argv[] = {arg0, nullptr};
            static QApplication* app = new QApplication(argc, argv);
            Q_UNUSED(app);
        }
        TranslationManager::instance().initialize(qApp);
        TranslationManager::instance().set_language(Language::English);
    }
};
