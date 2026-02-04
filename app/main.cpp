#include "AppInfo.hpp"
#include "EmbeddedEnv.hpp"
#include "Logger.hpp"
#include "MainApp.hpp"
#include "Utils.hpp"
#include "LLMSelectionDialog.hpp"
#include <app_version.hpp>

#include <QApplication>
#include <QDialog>
#include <QGuiApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QSize>
#include <QElapsedTimer>
#include <QTimer>

#include <functional>
#include <algorithm>
#include <vector>
#include <cstring>
#include <array>
#include <cstdlib>
#include <filesystem>
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
    bool console_log{false};
    bool force_direct_run{false};
    std::vector<char*> qt_args;
};

ParsedArguments parse_command_line(int argc, char** argv)
{
    ParsedArguments parsed;
    parsed.qt_args.reserve(static_cast<size_t>(argc) + 1);

    for (int i = 0; i < argc; ++i) {
        const bool is_flag = (i > 0);
        if (is_flag && std::strcmp(argv[i], "--development") == 0) {
            parsed.development_mode = true;
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
        parsed.qt_args.push_back(argv[i]);
    }
    parsed.qt_args.push_back(nullptr);
    return parsed;
}

#if defined(__APPLE__)
#ifndef AI_FILE_SORTER_GGML_SUBDIR
#define AI_FILE_SORTER_GGML_SUBDIR "precompiled"
#endif

bool ends_with(const std::string& value, const std::string& suffix)
{
    if (suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

bool has_ggml_payload(const std::filesystem::path& dir)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        if (filename.rfind("libggml-", 0) != 0) {
            continue;
        }
        if (ends_with(filename, ".so") || ends_with(filename, ".dylib")) {
            return true;
        }
    }
    return false;
}

void ensure_ggml_backend_dir()
{
    const char* current = std::getenv("AI_FILE_SORTER_GGML_DIR");
    if (current && current[0] != '\0') {
        return;
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

    const std::filesystem::path exe_dir = exe_path.parent_path();
    const std::filesystem::path ggml_subdir(AI_FILE_SORTER_GGML_SUBDIR);
    const std::array<std::filesystem::path, 6> candidates = {
        exe_dir / "../lib" / ggml_subdir,
        exe_dir / "../../lib" / ggml_subdir,
        exe_dir / "../lib",
        exe_dir / "../../lib",
        std::filesystem::path("/usr/local/lib"),
        std::filesystem::path("/opt/homebrew/lib")
    };

    for (const auto& candidate : candidates) {
        if (has_ggml_payload(candidate)) {
            setenv("AI_FILE_SORTER_GGML_DIR", candidate.string().c_str(), 1);
            break;
        }
    }
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
    QPixmap splash_pix(QStringLiteral(":/net/quicknode/AIFileSorter/images/icon_512x512.png"));
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

bool has_local_model_for_env(const char* env_key)
{
    if (!env_key) {
        return false;
    }
    const char* url = std::getenv(env_key);
    if (!url || *url == '\0') {
        return false;
    }
    try {
        const std::string path = Utils::make_default_path_to_file_from_download_url(url);
        return file_exists(path);
    } catch (...) {
        return false;
    }
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

    const char* env_var = nullptr;
    switch (choice) {
        case LLMChoice::Local_3b:
            env_var = "LOCAL_LLM_3B_DOWNLOAD_URL";
            break;
        case LLMChoice::Local_3b_legacy:
            env_var = "LOCAL_LLM_3B_LEGACY_DOWNLOAD_URL";
            break;
        case LLMChoice::Local_7b:
            env_var = "LOCAL_LLM_7B_DOWNLOAD_URL";
            break;
        default:
            break;
    }
    return has_local_model_for_env(env_var);
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

int run_application(const ParsedArguments& parsed_args)
{
    EmbeddedEnv env_loader(":/net/quicknode/AIFileSorter/.env");
    env_loader.load_env();
#if defined(__APPLE__)
    ensure_ggml_backend_dir();
#endif
    setlocale(LC_ALL, "");
    const std::string locale_path = Utils::get_executable_path() + "/locale";
    bindtextdomain("net.quicknode.AIFileSorter", locale_path.c_str());

    const QString display_name = app_display_name();
    QCoreApplication::setApplicationName(display_name);
    QGuiApplication::setApplicationDisplayName(display_name);

    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QApplication app(qt_argc, qt_argv);

    Settings settings;
    settings.load();

    const auto finish_splash = [&]() {};

    if (!ensure_llm_choice(settings, finish_splash)) {
        return EXIT_SUCCESS;
    }

    MainApp main_app(settings, parsed_args.development_mode);
    main_app.run();

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
