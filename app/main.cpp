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
#include <QPainter>

#include <curl/curl.h>
#include <locale.h>
#include <libintl.h>
#include <cstdio>
#include <iostream>


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


int main(int argc, char **argv) {
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
        EmbeddedEnv env_loader(":/net/quicknode/AIFileSorter/.env");
        env_loader.load_env();
        setlocale(LC_ALL, "");
        std::string locale_path = Utils::get_executable_path() + "/locale";
        bindtextdomain("net.quicknode.AIFileSorter", locale_path.c_str());

        QCoreApplication::setApplicationName(QStringLiteral("AI File Sorter"));
        QGuiApplication::setApplicationDisplayName(QStringLiteral("AI File Sorter"));

        QApplication app(argc, argv);

        QPixmap splash_pix(QStringLiteral(":/net/quicknode/AIFileSorter/images/icon_512x512.png"));
        if (splash_pix.isNull()) {
            splash_pix = QPixmap(256, 256);
            splash_pix.fill(Qt::black);
        }
        const QSize base_size = QSize(320, 320);
        const QSize padded_size = QSize(static_cast<int>(base_size.width() * 1.2),
                                        static_cast<int>(base_size.height() * 1.1));

        QPixmap scaled_splash = splash_pix.scaled(base_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap splash_canvas(padded_size);
        splash_canvas.fill(QColor(QStringLiteral("#f5e6d3"))); // matte beige background

        QPainter painter(&splash_canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QPoint centered_icon((padded_size.width() - scaled_splash.width()) / 2,
                                   (padded_size.height() - scaled_splash.height()) / 2 - 10);
        painter.drawPixmap(centered_icon, scaled_splash);
        painter.end();

        QSplashScreen splash(splash_canvas);
        splash.setWindowFlag(Qt::WindowStaysOnTopHint);
        splash.setWindowFlag(Qt::SplashScreen);
        const QString splash_text = QStringLiteral("AI File Sorter %1").arg(QString::fromStdString(APP_VERSION.to_string()));
        splash.showMessage(splash_text, Qt::AlignBottom | Qt::AlignHCenter, Qt::black);
        splash.show();
        splash.raise();
        splash.activateWindow();
        QElapsedTimer splash_timer;
        splash_timer.start();
        app.processEvents();

        bool splash_finished = false;
        QWidget* splash_target_widget = nullptr;
        std::function<void()> finishSplash = [&]() {
            if (!splash_finished) {
                splash.finish(splash_target_widget);
                splash_finished = true;
            }
        };
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &splash, finishSplash);

        Settings settings;
        settings.load();

        if (settings.get_llm_choice() == LLMChoice::Unset) {
            LLMSelectionDialog llm_dialog(settings);
            if (llm_dialog.exec() != QDialog::Accepted) {
                finishSplash();
                return EXIT_SUCCESS;
            }

            settings.set_llm_choice(llm_dialog.get_selected_llm_choice());
            settings.save();
        }

        MainApp main_app(settings);
        splash_target_widget = &main_app;

        constexpr int splash_duration_ms = 3000;
        const qint64 elapsed_ms = splash_timer.elapsed();
        const int remaining_ms = std::max(0, splash_duration_ms - static_cast<int>(elapsed_ms));
        splash.raise();
        splash.activateWindow();
        QTimer::singleShot(0, [&splash]() {
            splash.raise();
            splash.activateWindow();
        });
        QTimer::singleShot(remaining_ms, &splash, finishSplash);
        main_app.run();

        int result = app.exec();
        finishSplash();
        main_app.shutdown();
        return result;
    } catch (const std::exception& ex) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->critical("Error: {}", ex.what());
        } else {
            std::fprintf(stderr, "Error: %s\n", ex.what());
        }
        return EXIT_FAILURE;
    }
}
