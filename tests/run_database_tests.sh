#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/build"
mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/spdlog/sinks"
mkdir -p "$BUILD_DIR/spdlog/fmt"

TEST_SRC="$BUILD_DIR/database_manager_test.cpp"
STUB_SRC="$BUILD_DIR/logger_stub.cpp"
OUTPUT="$BUILD_DIR/database_manager_test"

cat > "$TEST_SRC" <<'CPP'
#include "DatabaseManager.hpp"
#include "Types.hpp"

#include <filesystem>
#include <iostream>
#include <set>
#include <cstdlib>
#include <chrono>
#include <system_error>
#include <vector>

namespace {
struct TempDir {
    explicit TempDir(std::filesystem::path p) : path(std::move(p)) {}
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::filesystem::path path;
};

[[noreturn]] void fail(const std::string& message) {
    std::cerr << message << '\n';
    std::exit(1);
}
} // namespace

int main() {
    const auto unique_dir = std::filesystem::temp_directory_path() /
        ("aifs-dbtest-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(unique_dir);
    TempDir guard(unique_dir);

    DatabaseManager manager(unique_dir.string());

    const std::string test_dir = "/sample";
    DatabaseManager::ResolvedCategory valid{0, "Docs", "Manuals"};
    DatabaseManager::ResolvedCategory empty_cat{0, "", ""};
    DatabaseManager::ResolvedCategory whitespace_cat{0, "   ", "   "};

    if (!manager.insert_or_update_file_with_categorization("valid.txt", "F", test_dir, valid)) {
        fail("Failed to insert valid row");
    }
    if (!manager.insert_or_update_file_with_categorization("empty.txt", "F", test_dir, empty_cat)) {
        fail("Failed to insert empty row");
    }
    if (!manager.insert_or_update_file_with_categorization("space.txt", "F", test_dir, whitespace_cat)) {
        fail("Failed to insert whitespace row");
    }

    auto removed = manager.remove_empty_categorizations(test_dir);
    if (removed.size() != 2) {
        fail("Expected 2 entries removed, got " + std::to_string(removed.size()));
    }

    std::set<std::string> removed_names;
    for (const auto& entry : removed) {
        removed_names.insert(entry.file_name);
    }
    if (!removed_names.contains("empty.txt") || !removed_names.contains("space.txt")) {
        fail("Unexpected entries removed");
    }

    if (!manager.remove_empty_categorizations(test_dir).empty()) {
        fail("Subsequent cleanup should find no additional entries");
    }

    auto empty_lookup = manager.get_categorization_from_db("empty.txt", FileType::File);
    if (!empty_lookup.empty()) {
        fail("Empty entry still present after cleanup");
    }
    auto whitespace_lookup = manager.get_categorization_from_db("space.txt", FileType::File);
    if (!whitespace_lookup.empty()) {
        fail("Whitespace entry still present after cleanup");
    }

    auto remaining = manager.get_categorized_files(test_dir);
    if (remaining.size() != 1 || remaining.front().file_name != "valid.txt" ||
        remaining.front().category != "Docs" || remaining.front().subcategory != "Manuals") {
        fail("Valid entry was not preserved during cleanup");
    }

    std::cout << "Database manager cleanup test passed" << std::endl;
    return 0;
}
CPP

cat > "$STUB_SRC" <<'CPP'
#include "Logger.hpp"
#include <filesystem>

std::string Logger::get_log_directory() {
    return std::filesystem::temp_directory_path().string();
}

void Logger::setup_loggers() {}

std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string&) {
    return nullptr;
}

std::string Logger::get_log_file_path(const std::string& log_dir, const std::string& log_name) {
    return log_dir + "/" + log_name;
}
CPP

cat > "$BUILD_DIR/spdlog/spdlog.h" <<'CPP'
#pragma once
#include <chrono>
#include <memory>
#include <string>

namespace spdlog {
namespace level {
enum level_enum {
    trace,
    debug,
    info,
    warn,
    err,
    critical,
    off
};
}

class logger {
public:
    template <typename... Args>
    void log(level::level_enum, const std::string&, Args&&...) {}

    template <typename... Args>
    void info(const std::string&, Args&&...) {}

    template <typename... Args>
    void warn(const std::string&, Args&&...) {}

    template <typename... Args>
    void error(const std::string&, Args&&...) {}

    void flush_on(level::level_enum) {}
    void set_level(level::level_enum) {}
};

inline std::shared_ptr<logger> get(const std::string&) { return nullptr; }
inline void register_logger(std::shared_ptr<logger>) {}
inline void flush_every(std::chrono::seconds) {}
inline void set_level(level::level_enum) {}
inline void info(const std::string&) {}
} // namespace spdlog
CPP

cat > "$BUILD_DIR/spdlog/sinks/stdout_color_sinks.h" <<'CPP'
#pragma once
namespace spdlog { namespace sinks {
class stdout_color_sink_mt {};
}} // namespace spdlog::sinks
CPP

cat > "$BUILD_DIR/spdlog/sinks/basic_file_sink.h" <<'CPP'
#pragma once
namespace spdlog { namespace sinks {
class basic_file_sink_mt {};
}} // namespace spdlog::sinks
CPP

cat > "$BUILD_DIR/spdlog/sinks/rotating_file_sink.h" <<'CPP'
#pragma once
namespace spdlog { namespace sinks {
class rotating_file_sink_mt {};
}} // namespace spdlog::sinks
CPP

cat > "$BUILD_DIR/spdlog/fmt/fmt.h" <<'CPP'
#pragma once
#include <string>

namespace fmt {
inline const std::string& runtime(const std::string& value) {
    return value;
}

template <typename... Args>
std::string format(const std::string& fmt_str, Args&&...) {
    return fmt_str;
}
} // namespace fmt
CPP

INCLUDES=(
    -I"$BUILD_DIR"
    -I"$ROOT_DIR/app/include"
)

LIBS=(
    -lsqlite3
    -pthread
)

g++ -std=c++20 -fPIC "${INCLUDES[@]}" \
    "$TEST_SRC" "$STUB_SRC" \
    "$ROOT_DIR/app/lib/DatabaseManager.cpp" \
    -o "$OUTPUT" "${LIBS[@]}"

"$OUTPUT"
