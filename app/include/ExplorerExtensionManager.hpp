/**
 * @file ExplorerExtensionManager.hpp
 * @brief App-side Windows Explorer extension detection and launch helpers.
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>

/**
 * @brief Detects and opens the optional Windows Explorer extension from the main app.
 *
 * This class deliberately does not modify extension settings or registration. The extension
 * remains responsible for connecting itself to an AI File Sorter executable.
 */
class ExplorerExtensionManager {
public:
    /**
     * @brief High-level extension availability state used by app menus and prompts.
     */
    enum class State {
        UnsupportedPlatform,
        NotInstalled,
        Installed,
        InstalledNeedsRepair
    };

    /**
     * @brief Detailed result from extension detection.
     */
    struct Status {
        State state{State::UnsupportedPlatform};
        std::optional<std::filesystem::path> progress_executable;
        std::optional<std::filesystem::path> extension_dll;
        bool package_registered{false};
    };

    /**
     * @brief Inspects the current user installation state.
     * @return Extension status for the current platform/user.
     */
    Status inspect() const;
    /**
     * @brief Returns the high-level extension state.
     * @return Current extension state.
     */
    State state() const;
    /**
     * @brief Returns whether the extension settings window can be launched.
     * @return True when the progress/settings executable is available.
     */
    bool can_open_settings() const;
    /**
     * @brief Returns whether the extension activity window can be launched.
     * @return True when the progress executable is available.
     */
    bool can_open_activity_window() const;
    /**
     * @brief Opens the extension install/download page.
     * @param error Optional error detail populated on failure.
     * @return True when the request was handed to the operating system.
     */
    bool open_install_page(std::string* error = nullptr) const;
    /**
     * @brief Opens the extension settings window.
     * @param error Optional error detail populated on failure.
     * @return True when the settings process was launched.
     */
    bool open_settings(std::string* error = nullptr) const;
    /**
     * @brief Opens the extension activity/progress window.
     * @param error Optional error detail populated on failure.
     * @return True when the activity process was launched.
     */
    bool open_activity_window(std::string* error = nullptr) const;
};
