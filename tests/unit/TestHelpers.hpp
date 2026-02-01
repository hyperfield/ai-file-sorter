/**
 * @file TestHelpers.hpp
 * @brief Common utilities for unit tests (temp paths, env guards, Qt context).
 */
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

/**
 * @brief Build a unique token string with the given prefix.
 * @param prefix Prefix to include in the token.
 * @return Unique token string that is safe for filenames.
 */
inline std::string make_unique_token(std::string_view prefix) {
    static std::atomic<uint64_t> counter{0};
    const uint64_t value = counter.fetch_add(1, std::memory_order_relaxed);
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::string(prefix) + std::to_string(now) + "-" + std::to_string(value);
}

/**
 * @brief RAII helper that sets and restores environment variables.
 */
class EnvVarGuard {
public:
    /**
     * @brief Set or unset an environment variable for the guard lifetime.
     * @param key Environment variable name.
     * @param value New value; unset when std::nullopt.
     */
    EnvVarGuard(std::string key, std::optional<std::string> value)
        : key_(std::move(key)) {
        if (const char* existing = std::getenv(key_.c_str())) {
            original_ = existing;
        }
        apply(value);
    }

    /**
     * @brief Restore the original environment variable state.
     */
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

/**
 * @brief Creates a temporary directory and cleans it up on destruction.
 */
class TempDir {
public:
    /**
     * @brief Create a unique temporary directory.
     */
    TempDir()
        : path_(std::filesystem::temp_directory_path() /
                make_unique_token("aifs-test-")) {
        std::filesystem::create_directories(path_);
    }

    /**
     * @brief Remove the temporary directory and its contents.
     */
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    /**
     * @brief Return the temporary directory path.
     * @return Reference to the directory path.
     */
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/**
 * @brief Creates a temporary GGUF-like file for model-related tests.
 */
class TempModelFile {
public:
    /**
     * @brief Create a temporary model file with minimal GGUF metadata.
     * @param block_count GGUF block count to encode.
     * @param file_size Total file size in bytes.
     */
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

    /**
     * @brief Remove the temporary model file.
     */
    ~TempModelFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    /**
     * @brief Return the temporary model file path.
     * @return Reference to the file path.
     */
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/**
 * @brief Ensures a QApplication and translation manager are initialized.
 */
class QtAppContext {
public:
    /**
     * @brief Initialize QApplication (if needed) and load translations.
     */
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
