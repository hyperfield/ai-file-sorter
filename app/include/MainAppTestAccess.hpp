#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "Types.hpp"

#include <QString>
#include <functional>
#include <QCheckBox>
#include <unordered_set>
#include <vector>

class MainApp;
class Settings;

class MainAppTestAccess {
public:
    enum class SimulatedSupportResult { Support, NotSure, CannotDonate };
    static QString analyze_button_text(const MainApp& app);
    static QString path_label_text(const MainApp& app);
    static QCheckBox* analyze_images_checkbox(MainApp& app);
    static QCheckBox* process_images_only_checkbox(MainApp& app);
    static QCheckBox* offer_rename_images_checkbox(MainApp& app);
    static QCheckBox* rename_images_only_checkbox(MainApp& app);
    static void split_entries_for_analysis(const std::vector<FileEntry>& files,
                                           bool analyze_images,
                                           bool process_images_only,
                                           bool rename_images_only,
                                           const std::unordered_set<std::string>& renamed_files,
                                           std::vector<FileEntry>& image_entries,
                                           std::vector<FileEntry>& other_entries);
    static void set_visual_llm_available_probe(MainApp& app, std::function<bool()> probe);
    static void set_llm_selection_runner(MainApp& app, std::function<void()> runner);
    static void set_image_analysis_prompt_override(MainApp& app, std::function<bool()> prompt);
    static void trigger_retranslate(MainApp& app);
    static void add_categorized_files(MainApp& app, int count);
    static void simulate_support_prompt(Settings& settings,
                                        bool& prompt_state,
                                        int count,
                                        std::function<SimulatedSupportResult(int)> callback);
};

#endif // AI_FILE_SORTER_TEST_BUILD
