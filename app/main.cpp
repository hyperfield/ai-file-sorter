#include "AppInfo.hpp"
#include "AppTestRunner.hpp"
#include "EmbeddedEnv.hpp"
#include "GgmlRuntimePaths.hpp"
#include "HeadlessAnalysisCommand.hpp"
#include "ImageAnalyzerFactory.hpp"
#include "ImageAnalyzer.hpp"
#include "Logger.hpp"
#include "LlmCatalog.hpp"
#include "MainApp.hpp"
#include "SingleInstanceCoordinator.hpp"
#include "UpdaterBuildConfig.hpp"
#include "UpdaterLaunchOptions.hpp"
#include "UpdaterLiveTestConfig.hpp"
#include "Utils.hpp"
#include "LLMSelectionDialog.hpp"
#include "VisualLlmRuntime.hpp"
#include <app_version.hpp>

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QGuiApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QSize>
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <QPainter>
#include <memory>

#include <curl/curl.h>
#include <locale.h>
#include <libintl.h>
#include <cstdio>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif
using SetProcessDpiAwarenessContextFn = BOOL (WINAPI *)(HANDLE);
using SetProcessDpiAwarenessFn = HRESULT (WINAPI *)(int); // 2 = PROCESS_PER_MONITOR_DPI_AWARE
#endif


bool initialize_loggers()
{
    try {
        Logger::setup_loggers();
        return true;
    } catch (const std::exception &e) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->critical("Failed to initialize loggers: {}", e.what());
        } else {
            std::fprintf(stderr, "Failed to initialize loggers: %s\n", e.what());
        }
        return false;
    }
}

namespace {

struct ParsedArguments {
    bool development_mode{false};
    bool test_mode{false};
    bool console_log{false};
    bool force_direct_run{false};
    bool show_review_history{false};
    std::optional<std::string> self_test_suite;
    std::optional<std::string> visual_gpu_probe_backend;
    HeadlessAnalysisCommand::ParseResult headless;
    UpdaterLiveTestConfig updater_live_test;
    std::vector<char*> qt_args;
};

bool consume_prefixed_value(const char* argument,
                            const char* prefix,
                            std::optional<std::string>& target)
{
    const std::size_t prefix_length = std::strlen(prefix);
    if (std::strncmp(argument, prefix, prefix_length) != 0) {
        return false;
    }
    target = std::string(argument + prefix_length);
    return true;
}

bool env_has_value(const char* key)
{
    const char* value = std::getenv(key);
    return value && value[0] != '\0';
}

void set_process_env(const char* key, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

void apply_updater_live_test_environment(const UpdaterLiveTestConfig& args)
{
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
        set_process_env(UpdaterLaunchOptions::kLiveTestVersionEnv,
                        APP_VERSION.to_numeric_string() + ".1");
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

ParsedArguments parse_command_line(int argc, char** argv)
{
    ParsedArguments parsed;
    parsed.headless = HeadlessAnalysisCommand::parse(argc, argv);
    parsed.qt_args.reserve(static_cast<size_t>(argc) + 1);

    for (int i = 0; i < argc; ++i) {
        if (i > 0 &&
            static_cast<std::size_t>(i) < parsed.headless.consumed_arguments.size() &&
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
            (std::strcmp(argv[i], "--show-review-history") == 0 ||
             std::strcmp(argv[i], "--review-history") == 0 ||
             std::strcmp(argv[i], "--action-history") == 0)) {
            parsed.show_review_history = true;
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
        if (is_flag &&
            consume_prefixed_value(argv[i], "--visual-gpu-probe=", parsed.visual_gpu_probe_backend)) {
            continue;
        }
        if (is_flag && std::strcmp(argv[i], UpdaterLaunchOptions::kLiveTestFlag) == 0) {
            parsed.updater_live_test.enabled = true;
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i],
                                              UpdaterLaunchOptions::kLiveTestUrlFlag,
                                              parsed.updater_live_test.installer_url)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i],
                                              UpdaterLaunchOptions::kLiveTestSha256Flag,
                                              parsed.updater_live_test.installer_sha256)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i],
                                              UpdaterLaunchOptions::kLiveTestVersionFlag,
                                              parsed.updater_live_test.current_version)) {
            continue;
        }
        if (is_flag && consume_prefixed_value(argv[i],
                                              UpdaterLaunchOptions::kLiveTestMinVersionFlag,
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

void ensure_ggml_backend_dir()
{
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

    const auto resolved = GgmlRuntimePaths::resolve_macos_backend_dir(
        current_dir,
        exe_path,
        AI_FILE_SORTER_GGML_SUBDIR);
    if (!resolved) {
        return;
    }

    setenv("AI_FILE_SORTER_GGML_DIR", resolved->string().c_str(), 1);
}
#endif

#ifdef _WIN32
bool allow_direct_launch(int argc, char** argv)
{
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

bool file_exists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

void ensure_windows_ggml_backend_env()
{
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
    const bool has_root_ggml =
        file_exists(exe_dir / "ggml.dll") &&
        (file_exists(exe_dir / "ggml-cpu.dll") ||
         file_exists(exe_dir / "ggml-vulkan.dll") ||
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

void enable_per_monitor_dpi_awareness()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        const auto set_ctx = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (set_ctx && set_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }
    HMODULE shcore = LoadLibraryW(L"Shcore.dll");
    if (shcore) {
        const auto set_awareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
            GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (set_awareness) {
            // 2 == PROCESS_PER_MONITOR_DPI_AWARE
            set_awareness(2);
        }
        FreeLibrary(shcore);
    }
}

void attach_console_if_requested(bool enable)
{
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

[[maybe_unused]] QPixmap build_splash_pixmap()
{
    QPixmap splash_pix(QStringLiteral(":/dev/hfstudio/AIFileSorter/images/icon_512x512.png"));
    if (splash_pix.isNull()) {
        splash_pix = QPixmap(256, 256);
        splash_pix.fill(Qt::black);
    }

    const QSize base_size(320, 320);
    const QSize padded_size(static_cast<int>(base_size.width() * 1.2),
                            static_cast<int>(base_size.height() * 1.1));

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
    explicit SplashController(QApplication& app)
        : app_(app)
    {
        Q_UNUSED(app_);
    }

    void set_target(QWidget* target)
    {
        target_ = target;
    }

    void keep_visible_for(int minimum_duration_ms)
    {
        Q_UNUSED(minimum_duration_ms);
    }

    void finish()
    {
        finished_ = true;
    }

private:
    QApplication& app_;
    bool finished_{false};
    QWidget* target_{nullptr};
};

bool file_exists(const std::string& path)
{
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

bool llm_choice_is_ready(const Settings& settings)
{
    const LLMChoice choice = settings.get_llm_choice();
    if (choice == LLMChoice::Unset) {
        return false;
    }
    if (choice == LLMChoice::Remote_OpenAI) {
        return !settings.get_openai_api_key().empty()
            && !settings.get_openai_model().empty();
    }
    if (choice == LLMChoice::Remote_Gemini) {
        return !settings.get_gemini_api_key().empty()
            && !settings.get_gemini_model().empty();
    }
    if (choice == LLMChoice::Remote_Custom) {
        const auto id = settings.get_active_custom_api_id();
        if (id.empty()) {
            return false;
        }
        const CustomApiEndpoint endpoint = settings.find_custom_api_endpoint(id);
        return !endpoint.id.empty()
            && !endpoint.base_url.empty()
            && !endpoint.model.empty();
    }
    if (choice == LLMChoice::Custom) {
        const auto id = settings.get_active_custom_llm_id();
        if (id.empty()) {
            return false;
        }
        const CustomLLM custom = settings.find_custom_llm(id);
        return !custom.id.empty()
            && !custom.path.empty()
            && file_exists(custom.path);
    }

    return builtin_llm_artifact_available(choice);
}

bool ensure_llm_choice(Settings& settings, const std::function<void()>& finish_splash)
{
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

QWidget* preferred_activation_target()
{
    if (QWidget* modal = QApplication::activeModalWidget()) {
        return modal;
    }
    if (QWidget* active = QApplication::activeWindow()) {
        return active;
    }

    const auto top_level_widgets = QApplication::topLevelWidgets();
    const auto it = std::find_if(top_level_widgets.cbegin(),
                                 top_level_widgets.cend(),
                                 [](QWidget* widget) {
                                     return widget && widget->isVisible();
                                 });
    return it != top_level_widgets.cend() ? *it : nullptr;
}

void activate_widget(QWidget* widget)
{
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

bool startup_command_opens_review_history(const QString& command)
{
    return command == QStringLiteral("show-review-history");
}

QString startup_command_from_arguments(const ParsedArguments& parsed_args)
{
    return parsed_args.show_review_history ? QStringLiteral("show-review-history") : QString();
}

void open_review_history_later(MainApp& main_app)
{
    QTimer::singleShot(0, &main_app, [&main_app]() {
        activate_widget(&main_app);
        main_app.open_review_history_dialog();
    });
}

void print_app_test_result(const AppTestRunner::Result& result)
{
    std::cout << "AI File Sorter self-test suite: " << result.suite << "\n";
    if (!result.error.empty()) {
        std::cout << "ERROR: " << result.error << "\n";
        return;
    }

    for (const auto& test_case : result.cases) {
        std::cout << (test_case.passed ? "[PASS] " : "[FAIL] ")
                  << test_case.name;
        if (!test_case.message.empty()) {
            std::cout << " - " << test_case.message;
        }
        std::cout << "\n";
    }
    std::cout << (result.passed() ? "Self-test result: PASS" : "Self-test result: FAIL")
              << "\n";
}

int run_self_test_mode(const ParsedArguments& parsed_args)
{
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

int run_visual_gpu_probe_mode(const ParsedArguments& parsed_args)
{
    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QCoreApplication app(qt_argc, qt_argv);

    set_process_env("AI_FILE_SORTER_VISUAL_SKIP_GPU_PREFLIGHT", "1");
    set_process_env("AI_FILE_SORTER_VISUAL_USE_GPU", "1");

    std::string error;
    const auto backend = VisualLlmRuntime::resolve_active_backend(
        parsed_args.visual_gpu_probe_backend.value_or(""),
        &error);
    if (!backend) {
        std::cerr << (error.empty() ? "Visual GPU probe could not resolve a backend." : error)
                  << "\n";
        return EXIT_FAILURE;
    }

    try {
        ImageAnalyzerSettings analyzer_settings;
        analyzer_settings.use_gpu = true;
        auto analyzer = ImageAnalyzerFactory::create(*backend, analyzer_settings);
        (void)analyzer;
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}

int run_headless_mode(const ParsedArguments& parsed_args)
{
    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QCoreApplication app(qt_argc, qt_argv);

    if (parsed_args.headless.help_requested) {
        std::cout << HeadlessAnalysisCommand::usage_text();
        return EXIT_SUCCESS;
    }
    if (!parsed_args.headless.error.empty()) {
        std::cerr << parsed_args.headless.error << "\n"
                  << HeadlessAnalysisCommand::usage_text();
        return HeadlessAnalysisCommand::Usage;
    }

    Settings settings;
    settings.load();
    const auto runtime_dir = Utils::utf8_to_path(settings.get_config_dir()) / "runtime";
    return HeadlessAnalysisCommand::run(parsed_args.headless.options,
                                        runtime_dir,
                                        std::cout,
                                        std::cerr);
}

int run_application(const ParsedArguments& parsed_args)
{
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
        load_missing_values_from_live_test_ini(
            updater_live_test,
            Utils::utf8_to_path(Utils::get_executable_path()));
        apply_updater_live_test_environment(updater_live_test);
    }

    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QApplication app(qt_argc, qt_argv);
    const QString instance_id = parsed_args.test_mode
        ? QStringLiteral("dev.hfstudio.AIFileSorter.Test")
        : QStringLiteral("dev.hfstudio.AIFileSorter");
    SingleInstanceCoordinator instance_guard(instance_id);
    const QString startup_command = startup_command_from_arguments(parsed_args);
    instance_guard.set_activation_message(startup_command);
    instance_guard.set_activation_callback([]() {
        activate_widget(preferred_activation_target());
    });
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

    if (!ensure_llm_choice(settings, finish_splash)) {
        return EXIT_SUCCESS;
    }

    MainApp main_app(settings,
                     parsed_args.development_mode || parsed_args.test_mode,
                     parsed_args.test_mode,
                     app_data_dir);
    instance_guard.set_activation_callback([&main_app](const QString& command) {
        activate_widget(preferred_activation_target());
        if (startup_command_opens_review_history(command)) {
            open_review_history_later(main_app);
        }
    });
    main_app.run();
    if (startup_command_opens_review_history(startup_command)) {
        open_review_history_later(main_app);
    }

    const int result = app.exec();
    main_app.shutdown();
    return result;
}

} // namespace


int main(int argc, char **argv) {

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
