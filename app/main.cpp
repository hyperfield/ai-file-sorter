#include <curl/curl.h>
#include <libintl.h>
#include <locale.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPainter>
#include <QPixmap>
#include <QSaveFile>
#include <QSize>
#include <QSplashScreen>
#include <QStyleHints>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <app_version.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "AppInfo.hpp"
#include "AppTestRunner.hpp"
#include "AppTheme.hpp"
#include "EmbeddedEnv.hpp"
#include "GgmlRuntimePaths.hpp"
#include "HeadlessAnalysisCommand.hpp"
#include "ImageAnalyzer.hpp"
#include "ImageAnalyzerFactory.hpp"
#include "LLMSelectionDialog.hpp"
#include "LlmCatalog.hpp"
#include "Logger.hpp"
#include "MainApp.hpp"
#include "PluginEntitlementService.hpp"
#include "SingleInstanceCoordinator.hpp"
#include "UpdaterBuildConfig.hpp"
#include "UpdaterLaunchOptions.hpp"
#include "UpdaterLiveTestConfig.hpp"
#include "Utils.hpp"
#include "VisualLlmRuntime.hpp"
#ifdef _WIN32
#include <windows.h>
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) - 4)
#endif
using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(int);  // 2 = PROCESS_PER_MONITOR_DPI_AWARE
#endif

bool initialize_loggers() {
    try {
        Logger::setup_loggers();
        return true;
    } catch (const std::exception& e) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->critical("Failed to initialize loggers: {}", e.what());
        } else {
            std::fprintf(stderr, "Failed to initialize loggers: %s\n", e.what());
        }
        return false;
    }
}

namespace {

struct PluginEntitlementCommand {
    bool info_requested{false};
    bool activate_requested{false};
    std::optional<std::string> activation_url;
    std::optional<std::string> license_key;
    std::optional<std::string> product_id;
    std::optional<std::string> app_version;
    std::optional<std::string> platform;
    std::optional<std::string> output_path;
};

struct ParsedArguments {
    bool development_mode{false};
    bool test_mode{false};
    bool console_log{false};
    bool force_direct_run{false};
    bool show_review_history{false};
    bool show_llm_selection{false};
    bool show_cache_maintenance{false};
    std::optional<std::string> self_test_suite;
    std::optional<std::string> visual_gpu_probe_backend;
    PluginEntitlementCommand plugin_entitlement;
    HeadlessAnalysisCommand::ParseResult headless;
    UpdaterLiveTestConfig updater_live_test;
    std::vector<char*> qt_args;
};

bool consume_prefixed_value(const char* argument, const char* prefix, std::optional<std::string>& target) {
    const std::size_t prefix_length = std::strlen(prefix);
    if (std::strncmp(argument, prefix, prefix_length) != 0) {
        return false;
    }
    target = std::string(argument + prefix_length);
    return true;
}

bool env_has_value(const char* key) {
    const char* value = std::getenv(key);
    return value && value[0] != '\0';
}

void set_process_env(const char* key, const std::string& value) {
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

void apply_updater_live_test_environment(const UpdaterLiveTestConfig& args) {
    if (!args.enabled) {
        return;
    }

    set_process_env(UpdaterLaunchOptions::kLiveTestModeEnv, "1");

    if (args.installer_url) {
        set_process_env(UpdaterLaunchOptions::kLiveTestUrlEnv, *args.installer_url);
    }
    if (args.installer_sha256) {
        set_process_env(UpdaterLaunchOptions::kLiveTestSha256Env, *args.installer_sha256);
    }
    if (args.current_version) {
        set_process_env(UpdaterLaunchOptions::kLiveTestVersionEnv, *args.current_version);
    } else if (!env_has_value(UpdaterLaunchOptions::kLiveTestVersionEnv)) {
        set_process_env(UpdaterLaunchOptions::kLiveTestVersionEnv, APP_VERSION.to_numeric_string() + ".1");
    }
    if (args.min_version) {
        set_process_env(UpdaterLaunchOptions::kLiveTestMinVersionEnv, *args.min_version);
    }

    if (!env_has_value(UpdaterLaunchOptions::kLiveTestUrlEnv)) {
        throw std::runtime_error(
            "--updater-live-test requires --updater-live-test-url or AI_FILE_SORTER_UPDATER_TEST_URL.");
    }
    if (!env_has_value(UpdaterLaunchOptions::kLiveTestSha256Env)) {
        throw std::runtime_error(
            "--updater-live-test requires --updater-live-test-sha256 or AI_FILE_SORTER_UPDATER_TEST_SHA256.");
    }
}

ParsedArguments parse_command_line(int argc, char** argv) {
    ParsedArguments parsed;
    parsed.headless = HeadlessAnalysisCommand::parse(argc, argv);
    parsed.qt_args.reserve(static_cast<size_t>(argc) + 1);

    for (int i = 0; i < argc; ++i) {
        if (i > 0 && static_cast<std::size_t>(i) < parsed.headless.consumed_arguments.size() &&
            parsed.headless.consumed_arguments[static_cast<std::size_t>(i)]) {
            continue;
        }
        const bool is_flag = (i > 0);
        if (is_flag && std::strcmp(argv[i], "--development") == 0) {
            parsed.development_mode = true;
            continue;
        }
        if (is_flag && std::strcmp(argv[i], "--test") == 0) {
            parsed.test_mode = true;
            continue;
        }
        if (is_flag && std::strcmp(argv[i], "--allow-direct-launch") == 0) {
            continue;
        }
        if (is_flag && std::strcmp(argv[i], "--console-log") == 0) {
            parsed.console_log = true;
            continue;
        }
        if (is_flag && std::strcmp(argv[i], "--force-direct-run") == 0) {
            parsed.force_direct_run = true;
            continue;
        }
        if (is_flag &&
            (std::strcmp(argv[i], "--show-review-history") == 0 || std::strcmp(argv[i], "--review-history") == 0 ||
             std::strcmp(argv[i], "--action-history") == 0)) {
            parsed.show_review_history = true;
            continue;
        }
        if (is_flag && (std::strcmp(argv[i], "--show-llm-selection") == 0 ||
                        std::strcmp(argv[i], "--select-llm") == 0 || std::strcmp(argv[i], "--llm-selection") == 0)) {
            parsed.show_llm_selection = true;
            continue;
        }
        if (is_flag && (std::strcmp(argv[i], "--show-cache-maintenance") == 0 ||
                        std::strcmp(argv[i], "--cache-maintenance") == 0 ||
                        std::strcmp(argv[i], "--clear-cache") == 0 || std::strcmp(argv[i], "--clean-cache") == 0)) {
            parsed.show_cache_maintenance = true;
            continue;
        }
        if (is_flag && std::strcmp(argv[i], "--self-test") == 0) {
            parsed.self_test_suite = "all";
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], "--self-test=", parsed.self_test_suite)) {
            if (parsed.self_test_suite->empty()) {
                parsed.self_test_suite = "all";
            }
            continue;
        }
        if (is_flag && std::strcmp(argv[i], "--visual-gpu-probe") == 0) {
            parsed.visual_gpu_probe_backend = std::string();
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], "--visual-gpu-probe=", parsed.visual_gpu_probe_backend)) {
            continue;
        }
        if (is_flag && std::strcmp(argv[i], "--plugin-entitlement-info") == 0) {
            parsed.plugin_entitlement.info_requested = true;
            continue;
        }
        if (is_flag && (std::strcmp(argv[i], "--activate-plugin-license") == 0 ||
                        std::strcmp(argv[i], "--plugin-entitlement-activate") == 0)) {
            parsed.plugin_entitlement.activate_requested = true;
            continue;
        }
        if (is_flag &&
            consume_prefixed_value(argv[i], "--plugin-activation-url=", parsed.plugin_entitlement.activation_url)) {
            continue;
        }
        if (is_flag &&
            consume_prefixed_value(argv[i], "--plugin-license-key=", parsed.plugin_entitlement.license_key)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], "--plugin-product-id=", parsed.plugin_entitlement.product_id)) {
            continue;
        }
        if (is_flag &&
            consume_prefixed_value(argv[i], "--plugin-app-version=", parsed.plugin_entitlement.app_version)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], "--plugin-platform=", parsed.plugin_entitlement.platform)) {
            continue;
        }
        if (is_flag &&
            consume_prefixed_value(argv[i], "--plugin-entitlement-output=", parsed.plugin_entitlement.output_path)) {
            continue;
        }
        if (is_flag && std::strcmp(argv[i], UpdaterLaunchOptions::kLiveTestFlag) == 0) {
            parsed.updater_live_test.enabled = true;
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], UpdaterLaunchOptions::kLiveTestUrlFlag,
                                              parsed.updater_live_test.installer_url)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], UpdaterLaunchOptions::kLiveTestSha256Flag,
                                              parsed.updater_live_test.installer_sha256)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], UpdaterLaunchOptions::kLiveTestVersionFlag,
                                              parsed.updater_live_test.current_version)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i], UpdaterLaunchOptions::kLiveTestMinVersionFlag,
                                              parsed.updater_live_test.min_version)) {
            continue;
        }
        parsed.qt_args.push_back(argv[i]);
    }
    parsed.qt_args.push_back(nullptr);
    return parsed;
}

#if defined(__APPLE__)
#ifndef AI_FILE_SORTER_GGML_SUBDIR
#define AI_FILE_SORTER_GGML_SUBDIR "precompiled"
#endif

void ensure_ggml_backend_dir() {
    std::optional<std::filesystem::path> current_dir;
    const char* current = std::getenv("AI_FILE_SORTER_GGML_DIR");
    if (current && current[0] != '\0') {
        current_dir = std::filesystem::path(current);
    }

    std::filesystem::path exe_path;
    try {
        exe_path = Utils::get_executable_path();
    } catch (const std::exception&) {
        return;
    }
    if (exe_path.empty()) {
        return;
    }

    const auto resolved =
        GgmlRuntimePaths::resolve_macos_backend_dir(current_dir, exe_path, AI_FILE_SORTER_GGML_SUBDIR);
    if (!resolved) {
        return;
    }

    setenv("AI_FILE_SORTER_GGML_DIR", resolved->string().c_str(), 1);
}
#endif

#ifdef _WIN32
bool allow_direct_launch(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--force-direct-run") == 0) {
            return true;
        }
    }
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--allow-direct-launch") == 0) {
            return true;
        }
    }
    return false;
}

bool file_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

void ensure_windows_ggml_backend_env() {
    auto logger = Logger::get_logger("core_logger");
    std::filesystem::path exe_path;
    try {
        exe_path = Utils::utf8_to_path(Utils::get_executable_path());
    } catch (const std::exception&) {
        return;
    }
    if (exe_path.empty()) {
        return;
    }

    const std::filesystem::path exe_dir = exe_path.parent_path();
    const bool has_root_ggml = file_exists(exe_dir / "ggml.dll") &&
                               (file_exists(exe_dir / "ggml-cpu.dll") || file_exists(exe_dir / "ggml-vulkan.dll") ||
                                file_exists(exe_dir / "ggml-cuda.dll"));
    if (!has_root_ggml) {
        return;
    }

    if (!env_has_value("AI_FILE_SORTER_GGML_DIR")) {
        set_process_env("AI_FILE_SORTER_GGML_DIR", Utils::path_to_utf8(exe_dir));
        if (logger) {
            logger->info("Configured AI_FILE_SORTER_GGML_DIR for direct-launch Windows runtime: '{}'",
                         Utils::path_to_utf8(exe_dir));
        }
    }

    if (env_has_value("AI_FILE_SORTER_GPU_BACKEND") || env_has_value("LLAMA_ARG_DEVICE")) {
        return;
    }

    const bool has_vulkan = file_exists(exe_dir / "ggml-vulkan.dll");
    const bool has_cuda = file_exists(exe_dir / "ggml-cuda.dll");

    if (has_vulkan && !has_cuda) {
        set_process_env("AI_FILE_SORTER_GPU_BACKEND", "vulkan");
        set_process_env("LLAMA_ARG_DEVICE", "vulkan");
        set_process_env("GGML_DISABLE_CUDA", "1");
        if (logger) {
            logger->info("Detected Vulkan-only root ggml payload; preferring Vulkan backend for direct launch.");
        }
    } else if (has_cuda && !has_vulkan) {
        set_process_env("AI_FILE_SORTER_GPU_BACKEND", "cuda");
        set_process_env("LLAMA_ARG_DEVICE", "cuda");
        if (logger) {
            logger->info("Detected CUDA-only root ggml payload; preferring CUDA backend for direct launch.");
        }
    }
}

void enable_per_monitor_dpi_awareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        const auto set_ctx =
            reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_ctx && set_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        const auto set_awareness =
            reinterpret_cast<SetProcessDpiAwarenessFn>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (set_awareness) {
            // 2 == PROCESS_PER_MONITOR_DPI_AWARE
            set_awareness(2);
        }
        FreeLibrary(shcore);
    }
}

void attach_console_if_requested(bool enable) {
    if (!enable) {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$", "r", stdin);
    }
}
#endif

[[maybe_unused]] QPixmap build_splash_pixmap() {
    QPixmap splash_pix(QStringLiteral(":/dev/hfstudio/AIFileSorter/images/icon_512x512.png"));
    if (splash_pix.isNull()) {
        splash_pix = QPixmap(256, 256);
        splash_pix.fill(Qt::black);
    }

    const QSize base_size(320, 320);
    const QSize padded_size(static_cast<int>(base_size.width() * 1.2), static_cast<int>(base_size.height() * 1.1));

    QPixmap scaled_splash = splash_pix.scaled(base_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap splash_canvas(padded_size);
    splash_canvas.fill(QColor(QStringLiteral("#f5e6d3")));

    QPainter painter(&splash_canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPoint centered_icon((padded_size.width() - scaled_splash.width()) / 2,
                               (padded_size.height() - scaled_splash.height()) / 2 - 10);
    painter.drawPixmap(centered_icon, scaled_splash);
    painter.end();

    return splash_canvas;
}

class SplashController {
   public:
    explicit SplashController(QApplication& app) : app_(app) { Q_UNUSED(app_); }

    void set_target(QWidget* target) { target_ = target; }

    void keep_visible_for(int minimum_duration_ms) { Q_UNUSED(minimum_duration_ms); }

    void finish() { finished_ = true; }

   private:
    QApplication& app_;
    bool finished_{false};
    QWidget* target_{nullptr};
};

bool file_exists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

bool llm_choice_is_ready(const Settings& settings) {
    const LLMChoice choice = settings.get_llm_choice();
    if (choice == LLMChoice::Unset) {
        return false;
    }
    if (choice == LLMChoice::Remote_OpenAI) {
        return !settings.get_openai_api_key().empty() && !settings.get_openai_model().empty();
    }
    if (choice == LLMChoice::Remote_Gemini) {
        return !settings.get_gemini_api_key().empty() && !settings.get_gemini_model().empty();
    }
    if (choice == LLMChoice::Remote_Custom) {
        const auto id = settings.get_active_custom_api_id();
        if (id.empty()) {
            return false;
        }
        const CustomApiEndpoint endpoint = settings.find_custom_api_endpoint(id);
        return !endpoint.id.empty() && !endpoint.base_url.empty() && !endpoint.model.empty();
    }
    if (choice == LLMChoice::Custom) {
        const auto id = settings.get_active_custom_llm_id();
        if (id.empty()) {
            return false;
        }
        const CustomLLM custom = settings.find_custom_llm(id);
        return !custom.id.empty() && !custom.path.empty() && file_exists(custom.path);
    }

    return builtin_llm_artifact_available(choice);
}

bool ensure_llm_choice(Settings& settings, const std::function<void()>& finish_splash) {
    if (llm_choice_is_ready(settings)) {
        return true;
    }

    LLMSelectionDialog llm_dialog(settings);
    if (llm_dialog.exec() != QDialog::Accepted) {
        if (finish_splash) {
            finish_splash();
        }
        return false;
    }

    settings.set_openai_api_key(llm_dialog.get_openai_api_key());
    settings.set_openai_model(llm_dialog.get_openai_model());
    settings.set_gemini_api_key(llm_dialog.get_gemini_api_key());
    settings.set_gemini_model(llm_dialog.get_gemini_model());
    settings.set_llm_choice(llm_dialog.get_selected_llm_choice());
    settings.set_llm_downloads_expanded(llm_dialog.get_llm_downloads_expanded());
    settings.set_llm_storage_dir(llm_dialog.get_llm_storage_dir());
    settings.set_visual_model_id(llm_dialog.get_selected_visual_model_id());
    if (llm_dialog.get_selected_llm_choice() == LLMChoice::Custom) {
        settings.set_active_custom_llm_id(llm_dialog.get_selected_custom_llm_id());
    } else {
        settings.set_active_custom_llm_id("");
    }
    if (llm_dialog.get_selected_llm_choice() == LLMChoice::Remote_Custom) {
        settings.set_active_custom_api_id(llm_dialog.get_selected_custom_api_id());
    } else {
        settings.set_active_custom_api_id("");
    }
    settings.save();
    return true;
}

QWidget* preferred_activation_target() {
    if (QWidget* modal = QApplication::activeModalWidget()) {
        return modal;
    }
    if (QWidget* active = QApplication::activeWindow()) {
        return active;
    }

    const auto top_level_widgets = QApplication::topLevelWidgets();
    const auto it = std::find_if(top_level_widgets.cbegin(), top_level_widgets.cend(),
                                 [](QWidget* widget) { return widget && widget->isVisible(); });
    return it != top_level_widgets.cend() ? *it : nullptr;
}

void activate_widget(QWidget* widget) {
    if (!widget) {
        return;
    }

    if (widget->isMinimized()) {
        widget->showNormal();
    } else {
        widget->show();
    }
    widget->raise();
    widget->activateWindow();

#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (hwnd) {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
    }
#endif
}

bool startup_command_opens_review_history(const QString& command) {
    return command == QStringLiteral("show-review-history");
}

bool startup_command_opens_llm_selection(const QString& command) {
    return command == QStringLiteral("show-llm-selection");
}

bool startup_command_opens_cache_maintenance(const QString& command) {
    return command == QStringLiteral("show-cache-maintenance");
}

QString startup_command_from_arguments(const ParsedArguments& parsed_args) {
    if (parsed_args.show_llm_selection) {
        return QStringLiteral("show-llm-selection");
    }
    if (parsed_args.show_cache_maintenance) {
        return QStringLiteral("show-cache-maintenance");
    }
    if (parsed_args.show_review_history) {
        return QStringLiteral("show-review-history");
    }
    return {};
}

void open_review_history_later(MainApp& main_app) {
    QTimer::singleShot(0, &main_app, [&main_app]() {
        activate_widget(&main_app);
        main_app.open_review_history_dialog();
    });
}

void open_llm_selection_later(MainApp& main_app) {
    QTimer::singleShot(0, &main_app, [&main_app]() {
        activate_widget(&main_app);
        main_app.show_llm_selection_dialog();
    });
}

void open_cache_maintenance_later(MainApp& main_app) {
    QTimer::singleShot(0, &main_app, [&main_app]() {
        activate_widget(&main_app);
        main_app.open_cache_cleanup_dialog();
    });
}

void print_app_test_result(const AppTestRunner::Result& result) {
    std::cout << "AI File Sorter self-test suite: " << result.suite << "\n";
    if (!result.error.empty()) {
        std::cout << "ERROR: " << result.error << "\n";
        return;
    }

    for (const auto& test_case : result.cases) {
        std::cout << (test_case.passed ? "[PASS] " : "[FAIL] ") << test_case.name;
        if (!test_case.message.empty()) {
            std::cout << " - " << test_case.message;
        }
        std::cout << "\n";
    }
    std::cout << (result.passed() ? "Self-test result: PASS" : "Self-test result: FAIL") << "\n";
}

bool plugin_entitlement_command_requested(const PluginEntitlementCommand& command) {
    return command.info_requested || command.activate_requested;
}

std::string default_plugin_activation_url() {
    const char* env_value = std::getenv("AI_FILE_SORTER_PLUGIN_ACTIVATION_URL");
    if (env_value && env_value[0] != '\0') {
        return std::string(env_value);
    }
    return "https://filesorter.app/api/plugins/activate/";
}

std::string default_plugin_platform() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

bool write_json_output(const QJsonObject& object, const std::optional<std::string>& output_path, std::string* error) {
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (output_path && !output_path->empty()) {
        QSaveFile file(QString::fromStdString(*output_path));
        if (!file.open(QIODevice::WriteOnly)) {
            if (error) {
                *error = file.errorString().toStdString();
            }
            return false;
        }
        file.write(bytes);
        if (!file.commit()) {
            if (error) {
                *error = file.errorString().toStdString();
            }
            return false;
        }
        return true;
    }

    std::cout.write(bytes.constData(), bytes.size());
    if (!bytes.endsWith('\n')) {
        std::cout << "\n";
    }
    return true;
}

QByteArray base64url_decode_cli_value(const QJsonValue& value) {
    if (!value.isString()) {
        return {};
    }
    QByteArray encoded = value.toString().trimmed().toUtf8();
    const qsizetype remainder = encoded.size() % 4;
    if (remainder > 0) {
        encoded.append(QByteArray(4 - remainder, '='));
    }
    return QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
}

std::size_t write_curl_response(char* contents, std::size_t size, std::size_t nmemb, void* user_data) {
    const std::size_t total_size = size * nmemb;
    auto* response = static_cast<std::string*>(user_data);
    response->append(contents, total_size);
    return total_size;
}

struct JsonHttpResponse {
    long status_code{0};
    std::string body;
};

JsonHttpResponse post_json(const std::string& url, const QJsonObject& payload) {
    struct CurlHandle {
        CURL* handle{curl_easy_init()};
        ~CurlHandle() {
            if (handle) {
                curl_easy_cleanup(handle);
            }
        }
    } curl;
    if (!curl.handle) {
        throw std::runtime_error("Failed to initialize cURL.");
    }

    struct HeaderList {
        curl_slist* list{nullptr};
        ~HeaderList() { curl_slist_free_all(list); }
        void append(const char* header) {
            curl_slist* next = curl_slist_append(list, header);
            if (!next) {
                throw std::runtime_error("Failed to allocate HTTP request headers.");
            }
            list = next;
        }
    } headers;
    headers.append("Accept: application/json");
    headers.append("Content-Type: application/json");

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    std::string response_body;
    char error_buffer[CURL_ERROR_SIZE] = {};
    std::string ca_bundle_path;

    curl_easy_setopt(curl.handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, body.constData());
    curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl.handle, CURLOPT_WRITEFUNCTION, write_curl_response);
    curl_easy_setopt(curl.handle, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl.handle, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl.handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.handle, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl.handle, CURLOPT_USERAGENT, "AI-File-Sorter-PluginActivator/1.0");

    if (url.rfind("https://", 0) == 0) {
        ca_bundle_path = Utils::path_to_utf8(Utils::ensure_ca_bundle());
        curl_easy_setopt(curl.handle, CURLOPT_CAINFO, ca_bundle_path.c_str());
    }

    const CURLcode result = curl_easy_perform(curl.handle);
    if (result != CURLE_OK) {
        const char* message = error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(result);
        throw std::runtime_error("Plugin activation request failed: " + std::string(message));
    }

    long status_code = 0;
    curl_easy_getinfo(curl.handle, CURLINFO_RESPONSE_CODE, &status_code);
    return JsonHttpResponse{status_code, std::move(response_body)};
}

std::string activation_error_message(const QJsonObject& response_object, const std::string& response_body) {
    std::string message = response_object.value("message").toString().trimmed().toStdString();
    if (!message.empty()) {
        return message;
    }
    message = response_object.value("error").toString().trimmed().toStdString();
    if (!message.empty()) {
        return message;
    }
    return response_body.substr(0, 500);
}

QJsonObject plugin_entitlement_info_json(const PluginEntitlementService& entitlement_service,
                                         const std::filesystem::path& config_dir) {
    QJsonObject object;
    object.insert("device_id", QString::fromStdString(entitlement_service.device_id()));
    object.insert("receipt_directory", QString::fromStdString(Utils::path_to_utf8(
                                           PluginEntitlementService::receipt_directory_for_config_dir(config_dir))));
    object.insert("trusted_entitlement_key_count",
                  static_cast<int>(PluginEntitlementService::default_public_keys().size()));
    object.insert("activation_url", QString::fromStdString(default_plugin_activation_url()));
    return object;
}

int run_plugin_entitlement_mode(const ParsedArguments& parsed_args) {
    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QCoreApplication app(qt_argc, qt_argv);

    Settings settings;
    settings.load();
    const auto config_dir = Utils::utf8_to_path(settings.get_config_dir());
    PluginEntitlementService entitlement_service(config_dir);
    const PluginEntitlementCommand& command = parsed_args.plugin_entitlement;

    if (command.info_requested && !command.activate_requested) {
        std::string output_error;
        if (!write_json_output(plugin_entitlement_info_json(entitlement_service, config_dir), command.output_path,
                               &output_error)) {
            std::cerr << "Failed to write plugin entitlement info: " << output_error << "\n";
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    const std::string license_key = command.license_key.value_or("");
    const std::string product_id = command.product_id.value_or("");
    if (license_key.empty() || product_id.empty()) {
        std::cerr << "--activate-plugin-license requires --plugin-license-key=<key> and "
                     "--plugin-product-id=<product_id>.\n";
        return EXIT_FAILURE;
    }

    QJsonObject request;
    request.insert("license_key", QString::fromStdString(license_key));
    request.insert("product_id", QString::fromStdString(product_id));
    request.insert("device_id", QString::fromStdString(entitlement_service.device_id()));
    request.insert("app_version",
                   QString::fromStdString(command.app_version.value_or(APP_VERSION.to_numeric_string())));
    request.insert("platform", QString::fromStdString(command.platform.value_or(default_plugin_platform())));

    const std::string activation_url = command.activation_url.value_or(default_plugin_activation_url());
    const JsonHttpResponse response = post_json(activation_url, request);

    QJsonParseError parse_error;
    const QJsonDocument response_doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body), &parse_error);
    const QJsonObject response_object = response_doc.isObject() ? response_doc.object() : QJsonObject();
    if (parse_error.error != QJsonParseError::NoError || !response_doc.isObject()) {
        std::cerr << "Plugin activation response was not valid JSON.\n";
        return EXIT_FAILURE;
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        std::cerr << "Plugin activation failed (HTTP " << response.status_code
                  << "): " << activation_error_message(response_object, response.body) << "\n";
        return EXIT_FAILURE;
    }

    const QByteArray payload = base64url_decode_cli_value(response_object.value("payload"));
    const QByteArray signature = base64url_decode_cli_value(response_object.value("signature"));
    const std::string key_id = response_object.value("key_id").toString().trimmed().toStdString();
    if (payload.isEmpty() || signature.size() != 64 || key_id.empty()) {
        std::cerr << "Plugin activation response did not include a complete signed receipt.\n";
        return EXIT_FAILURE;
    }

    std::string store_error;
    if (!entitlement_service.store_receipt(payload, signature, key_id, &store_error)) {
        std::cerr << "Plugin entitlement receipt was rejected: " << store_error << "\n";
        return EXIT_FAILURE;
    }

    QJsonObject output = plugin_entitlement_info_json(entitlement_service, config_dir);
    output.insert("activated", true);
    output.insert("product_id", QString::fromStdString(product_id));
    if (response_object.value("license").isObject()) {
        output.insert("license", response_object.value("license").toObject());
    }
    if (response_object.value("entitlements").isArray()) {
        output.insert("entitlements", response_object.value("entitlements").toArray());
    }
    std::string output_error;
    if (!write_json_output(output, command.output_path, &output_error)) {
        std::cerr << "Failed to write plugin activation result: " << output_error << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int run_self_test_mode(const ParsedArguments& parsed_args) {
    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QCoreApplication app(qt_argc, qt_argv);

    AppTestRunner runner;
    AppTestRunner::Options options;
    options.suite = parsed_args.self_test_suite.value_or("all");
    const auto result = runner.run(options);
    print_app_test_result(result);
    return result.passed() ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_visual_gpu_probe_mode(const ParsedArguments& parsed_args) {
    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QCoreApplication app(qt_argc, qt_argv);

    set_process_env("AI_FILE_SORTER_VISUAL_SKIP_GPU_PREFLIGHT", "1");
    set_process_env("AI_FILE_SORTER_VISUAL_USE_GPU", "1");

    std::string error;
    const auto backend =
        VisualLlmRuntime::resolve_active_backend(parsed_args.visual_gpu_probe_backend.value_or(""), &error);
    if (!backend) {
        std::cerr << (error.empty() ? "Visual GPU probe could not resolve a backend." : error) << "\n";
        return EXIT_FAILURE;
    }

    try {
        ImageAnalyzerSettings analyzer_settings;
        analyzer_settings.use_gpu = true;
        auto analyzer = ImageAnalyzerFactory::create(*backend, analyzer_settings);
        (void) analyzer;
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int run_headless_mode(const ParsedArguments& parsed_args) {
    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QCoreApplication app(qt_argc, qt_argv);

    if (parsed_args.headless.help_requested) {
        std::cout << HeadlessAnalysisCommand::usage_text();
        return EXIT_SUCCESS;
    }
    if (!parsed_args.headless.error.empty()) {
        std::cerr << parsed_args.headless.error << "\n" << HeadlessAnalysisCommand::usage_text();
        return HeadlessAnalysisCommand::Usage;
    }

    Settings settings;
    settings.load();
    const auto runtime_dir = Utils::utf8_to_path(settings.get_config_dir()) / "runtime";
    return HeadlessAnalysisCommand::run(parsed_args.headless.options, runtime_dir, std::cout, std::cerr);
}

int run_application(const ParsedArguments& parsed_args) {
    EmbeddedEnv env_loader(":/dev/hfstudio/AIFileSorter/.env");
    env_loader.load_env();
#if defined(__APPLE__)
    ensure_ggml_backend_dir();
#elif defined(_WIN32)
    ensure_windows_ggml_backend_env();
#endif
    setlocale(LC_ALL, "");
    const std::string locale_path = Utils::get_executable_path() + "/locale";
    bindtextdomain("dev.hfstudio.AIFileSorter", locale_path.c_str());

    const QString display_name = app_display_name();
    QCoreApplication::setApplicationName(display_name);
    QGuiApplication::setApplicationDisplayName(display_name);

    if (plugin_entitlement_command_requested(parsed_args.plugin_entitlement)) {
        return run_plugin_entitlement_mode(parsed_args);
    }
    if (parsed_args.self_test_suite) {
        return run_self_test_mode(parsed_args);
    }
    if (parsed_args.visual_gpu_probe_backend.has_value()) {
        return run_visual_gpu_probe_mode(parsed_args);
    }
    if (parsed_args.headless.requested) {
        return run_headless_mode(parsed_args);
    }

    auto updater_live_test = parsed_args.updater_live_test;
    if (!parsed_args.test_mode && UpdaterBuildConfig::update_checks_enabled()) {
        load_missing_values_from_live_test_ini(updater_live_test, Utils::utf8_to_path(Utils::get_executable_path()));
        apply_updater_live_test_environment(updater_live_test);
    }

    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
#ifdef _WIN32
    AppTheme::configure_windows_platform_dark_mode();
#endif
    QApplication app(qt_argc, qt_argv);
#ifdef _WIN32
    AppTheme::apply_windows_runtime_theme(app);
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app]() { AppTheme::apply_windows_runtime_theme(app); });
#endif
    const QString instance_id = parsed_args.test_mode ? QStringLiteral("dev.hfstudio.AIFileSorter.Test")
                                                      : QStringLiteral("dev.hfstudio.AIFileSorter");
    SingleInstanceCoordinator instance_guard(instance_id);
    const QString startup_command = startup_command_from_arguments(parsed_args);
    instance_guard.set_activation_message(startup_command);
    instance_guard.set_activation_callback([]() { activate_widget(preferred_activation_target()); });
    if (!instance_guard.acquire_primary_instance()) {
        return EXIT_SUCCESS;
    }

    Settings settings;
    settings.load();
    std::string app_data_dir;
    if (parsed_args.test_mode) {
        const auto profile_dir = Utils::utf8_to_path(settings.get_config_dir()) / "test_mode_profile";
        std::filesystem::create_directories(profile_dir);
        app_data_dir = Utils::path_to_utf8(profile_dir);
    }

    const auto finish_splash = [&]() {};

    const bool llm_selection_needed_at_startup = !llm_choice_is_ready(settings);
    if (!ensure_llm_choice(settings, finish_splash)) {
        return EXIT_SUCCESS;
    }
    const bool open_llm_selection_after_startup =
        startup_command_opens_llm_selection(startup_command) && !llm_selection_needed_at_startup;

    MainApp main_app(settings, parsed_args.development_mode || parsed_args.test_mode, parsed_args.test_mode,
                     app_data_dir);
    instance_guard.set_activation_callback([&main_app](const QString& command) {
        activate_widget(preferred_activation_target());
        if (startup_command_opens_llm_selection(command)) {
            open_llm_selection_later(main_app);
            return;
        }
        if (startup_command_opens_review_history(command)) {
            open_review_history_later(main_app);
            return;
        }
        if (startup_command_opens_cache_maintenance(command)) {
            open_cache_maintenance_later(main_app);
        }
    });
    main_app.run();
    if (open_llm_selection_after_startup) {
        open_llm_selection_later(main_app);
    }
    if (startup_command_opens_review_history(startup_command)) {
        open_review_history_later(main_app);
    }
    if (startup_command_opens_cache_maintenance(startup_command)) {
        open_cache_maintenance_later(main_app);
    }

    const int result = app.exec();
    main_app.shutdown();
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    ParsedArguments parsed = parse_command_line(argc, argv);

#ifdef _WIN32
    enable_per_monitor_dpi_awareness();
    attach_console_if_requested(parsed.console_log);
#endif

    if (!initialize_loggers()) {
        return EXIT_FAILURE;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    struct CurlCleanup {
        ~CurlCleanup() { curl_global_cleanup(); }
    } curl_cleanup;

#ifdef _WIN32
    _putenv("GSETTINGS_SCHEMA_DIR=schemas");
#endif
    try {
        return run_application(parsed);
    } catch (const std::exception& ex) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->critical("Error: {}", ex.what());
        } else {
            std::fprintf(stderr, "Error: %s\n", ex.what());
        }
        return EXIT_FAILURE;
    }
}
