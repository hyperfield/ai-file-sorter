#include "EmbeddedEnv.hpp"
#include "Logger.hpp"
#include "MainApp.hpp"
#include "Utils.hpp"
#include "LLMSelectionDialog.hpp"
#include <app_version.hpp>

#include <QApplication>
#include <QDialog>
#include <QSplashScreen>
#include <QPixmap>
#include <QSize>
#include <QElapsedTimer>
#include <QTimer>

#include <curl/curl.h>
#include <locale.h>
#include <libintl.h>
#include <cstdio>
#include <iostream>

#ifdef __linux__
        #include <X11/Xlib.h>
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

        QApplication app(argc, argv);

        QPixmap splash_pix(QStringLiteral(":/net/quicknode/AIFileSorter/images/icon_512x512.png"));
        if (splash_pix.isNull()) {
            splash_pix = QPixmap(256, 256);
            splash_pix.fill(Qt::black);
        }
        QPixmap scaled_splash = splash_pix.scaled(QSize(320, 320), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QSplashScreen splash(scaled_splash);
        splash.setWindowFlag(Qt::WindowStaysOnTopHint);
        const QString splash_text = QStringLiteral("AI File Sorter %1").arg(QString::fromStdString(APP_VERSION.to_string()));
        splash.showMessage(splash_text, Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
        splash.show();
        QElapsedTimer splash_timer;
        splash_timer.start();
        app.processEvents();

        Settings settings;
        settings.load();

        if (settings.get_llm_choice() == LLMChoice::Unset) {
            LLMSelectionDialog llm_dialog(settings);
            if (llm_dialog.exec() != QDialog::Accepted) {
                return EXIT_SUCCESS;
            }

            settings.set_llm_choice(llm_dialog.get_selected_llm_choice());
            settings.save();
        }

        MainApp main_app(settings);
        main_app.run();

        constexpr qint64 minimum_duration_ms = 4000;
        const qint64 elapsed_ms = splash_timer.elapsed();
        if (elapsed_ms < minimum_duration_ms) {
            const int remaining_ms = static_cast<int>(minimum_duration_ms - elapsed_ms);
            QTimer::singleShot(remaining_ms, &splash, [&splash, &main_app]() {
                splash.finish(&main_app);
            });
        } else {
            splash.finish(&main_app);
        }

        int result = app.exec();
        if (splash.isVisible()) {
            splash.finish(&main_app);
        }
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
