#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include <QString>
#include <functional>

class MainApp;
class Settings;

class MainAppTestAccess {
public:
    enum class SimulatedSupportResult { Support, NotSure, CannotDonate };
    static QString analyze_button_text(const MainApp& app);
    static QString path_label_text(const MainApp& app);
    static void trigger_retranslate(MainApp& app);
    static void add_categorized_files(MainApp& app, int count);
    static void simulate_support_prompt(Settings& settings,
                                        bool& prompt_state,
                                        int count,
                                        std::function<SimulatedSupportResult(int)> callback);
};

#endif // AI_FILE_SORTER_TEST_BUILD
