#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include <QString>

class MainApp;

class MainAppTestAccess {
public:
    static QString analyze_button_text(const MainApp& app);
    static QString path_label_text(const MainApp& app);
    static void trigger_retranslate(MainApp& app);
};

#endif // AI_FILE_SORTER_TEST_BUILD
