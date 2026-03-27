#pragma once

#include <string>

class MainApp;

/**
 * @brief Owns the end-to-end folder analysis workflow previously hosted in MainApp.
 */
class AnalysisCoordinator {
public:
    explicit AnalysisCoordinator(MainApp& app);

    void execute();

    static std::string resolve_document_prompt_name(const std::string& original_name,
                                                    const std::string& suggested_name);
    static std::string build_document_prompt_path(const std::string& full_path,
                                                  const std::string& prompt_name,
                                                  const std::string& summary);

private:
    MainApp& app_;
};
