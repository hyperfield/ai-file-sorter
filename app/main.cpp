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
#include <QPainter>
#include <memory>

#include <curl/curl.h>
#include <locale.h>
#include <libintl.h>
#include <cstdio>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
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
        parsed.qt_args.push_back(argv[i]);
    }
    parsed.qt_args.push_back(nullptr);
    return parsed;
}

#ifdef _WIN32
bool allow_direct_launch(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--allow-direct-launch") == 0) {
            return true;
        }
    }
    return false;
}
#endif

QPixmap build_splash_pixmap()
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
        : app_(app),
          splash_(std::make_unique<QSplashScreen>(build_splash_pixmap()))
    {
        splash_->setWindowFlag(Qt::WindowStaysOnTopHint);
        splash_->setWindowFlag(Qt::SplashScreen);
        const QString splash_text = QStringLiteral("AI File Sorter %1").arg(QString::fromStdString(APP_VERSION.to_string()));
        splash_->showMessage(splash_text, Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
        splash_->show();
        raise();
        QObject::connect(&app_, &QCoreApplication::aboutToQuit, splash_.get(), [this]() {
            finish();
        });
        timer_.start();
        app_.processEvents();
    }

    void set_target(QWidget* target)
    {
        target_ = target;
    }

    void keep_visible_for(int minimum_duration_ms)
    {
        const int elapsed_ms = static_cast<int>(timer_.elapsed());
        const int remaining_ms = std::max(0, minimum_duration_ms - elapsed_ms);
        raise();
        QTimer::singleShot(0, splash_.get(), [this]() {
            raise();
        });
        QTimer::singleShot(remaining_ms, splash_.get(), [this]() {
            finish();
        });
    }

    void finish()
    {
        if (finished_) {
            return;
        }
        splash_->finish(target_);
        finished_ = true;
    }

private:
    void raise()
    {
        splash_->raise();
        splash_->activateWindow();
    }

    QApplication& app_;
    std::unique_ptr<QSplashScreen> splash_;
    QElapsedTimer timer_;
    bool finished_{false};
    QWidget* target_{nullptr};
};

bool ensure_llm_choice(Settings& settings, const std::function<void()>& finish_splash)
{
    if (settings.get_llm_choice() != LLMChoice::Unset) {
        return true;
    }

    LLMSelectionDialog llm_dialog(settings);
    if (llm_dialog.exec() != QDialog::Accepted) {
        if (finish_splash) {
            finish_splash();
        }
        return false;
    }

    settings.set_llm_choice(llm_dialog.get_selected_llm_choice());
    settings.save();
    return true;
}

int run_application(int argc, char** argv)
{
    EmbeddedEnv env_loader(":/net/quicknode/AIFileSorter/.env");
    env_loader.load_env();
    setlocale(LC_ALL, "");
    const std::string locale_path = Utils::get_executable_path() + "/locale";
    bindtextdomain("net.quicknode.AIFileSorter", locale_path.c_str());

    QCoreApplication::setApplicationName(QStringLiteral("AI File Sorter"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("AI File Sorter"));

    ParsedArguments parsed_args = parse_command_line(argc, argv);
    int qt_argc = static_cast<int>(parsed_args.qt_args.size()) - 1;
    char** qt_argv = const_cast<char**>(parsed_args.qt_args.data());
    QApplication app(qt_argc, qt_argv);

    SplashController splash(app);

    Settings settings;
    settings.load();

    const auto finish_splash = [&]() {
        splash.finish();
    };

    if (!ensure_llm_choice(settings, finish_splash)) {
        return EXIT_SUCCESS;
    }

    MainApp main_app(settings, parsed_args.development_mode);
    splash.set_target(&main_app);
    constexpr int splash_duration_ms = 3000;
    splash.keep_visible_for(splash_duration_ms);
    main_app.run();

    const int result = app.exec();
    splash.finish();
    main_app.shutdown();
    return result;
}

} // namespace


int main(int argc, char **argv) {

#ifdef _WIN32
    if (!allow_direct_launch(argc, argv)) {
        const wchar_t* message =
            L"Please launch AI File Sorter by running StartAiFileSorter.exe.\n"
            L"The starter configures GPU backends and runtime DLLs automatically.";
        MessageBoxW(nullptr, message, L"AI File Sorter", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        return EXIT_FAILURE;
    }
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
        return run_application(argc, argv);
    } catch (const std::exception& ex) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->critical("Error: {}", ex.what());
        } else {
            std::fprintf(stderr, "Error: %s\n", ex.what());
        }
        return EXIT_FAILURE;
    }
}
