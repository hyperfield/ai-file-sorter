#include "EmbeddedEnv.hpp"
#include "Logger.hpp"
#include "MainApp.hpp"
#include "Utils.hpp"
#include "LLMSelectionDialog.hpp"

#include <QApplication>
#include <QDialog>

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

    #ifdef __linux__
        XInitThreads();
    #endif

    try {
        EmbeddedEnv env_loader(":/net/quicknode/AIFileSorter/.env");
        env_loader.load_env();
        setlocale(LC_ALL, "");
        std::string locale_path = Utils::get_executable_path() + "/locale";
        bindtextdomain("net.quicknode.AIFileSorter", locale_path.c_str());

        QApplication app(argc, argv);

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
        int result = app.exec();
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
