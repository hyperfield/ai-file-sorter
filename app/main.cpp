#include "EmbeddedEnv.hpp"
#include "Logger.hpp"
#include "MainApp.hpp"
#include "Utils.hpp"
#include <gio/gio.h>
#include <locale.h>
#include <libintl.h>
#include <iostream>
extern GResource *resources_get_resource();


bool initialize_loggers()
{
    try {
        Logger::setup_loggers();
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Failed to initialize loggers: " << e.what() << std::endl;
        return false;
    }
}


int main(int argc, char **argv)
{
    if (!initialize_loggers()) {
        return EXIT_FAILURE;
    }

    #ifdef _WIN32
        _putenv("GSETTINGS_SCHEMA_DIR=schemas");
    #endif

    try {
        g_resources_register(resources_get_resource());
        EmbeddedEnv env_loader("/net/quicknode/AIFileSorter/.env");
        env_loader.load_env();
        setlocale(LC_ALL, "");
        std::string locale_path = Utils::get_executable_path() + "/locale";
        bindtextdomain("net.quicknode.AIFileSorter", locale_path.c_str());
        MainApp app(argc, argv);
        app.run();
    } catch (const std::exception& ex) {
        g_critical("Error: %s", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}