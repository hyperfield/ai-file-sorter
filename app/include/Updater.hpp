#ifndef UPDATER_HPP
#define UPDATER_HPP

#include "Settings.hpp"
#include "UpdateFeed.hpp"
#include "UpdateInstaller.hpp"
#include "Version.hpp"
#include <functional>
#include <future>
#include <optional>
#include <string>

class QWidget;
class QString;

#ifdef AI_FILE_SORTER_TEST_BUILD
class UpdaterTestAccess;
#endif

class Updater
{
public:
    explicit Updater(Settings& settings);
    ~Updater();
    void begin();

private:
    Settings& settings;
    UpdateInstaller installer;
    const std::string update_spec_file_url;
    std::function<void(const std::string&)> open_download_url_fn_;
    std::function<void()> quit_fn_;
    std::optional<UpdateInfo> update_info;
    std::future<void> update_future;
    void check_updates();
    std::string fetch_update_metadata() const;
    Version string_to_Version(const std::string &version_str);
    void display_update_dialog(bool is_required=false);
    void show_required_update_dialog(const UpdateInfo& info, QWidget* parent);
    void show_optional_update_dialog(const UpdateInfo& info, QWidget* parent);
    bool is_update_available();
    bool is_update_required();
    bool is_update_skipped();
    bool trigger_update_action(const UpdateInfo& info, QWidget* parent, bool quit_after_open);
    UpdatePreparationResult prepare_installer_update(const UpdateInfo& info, QWidget* parent);
    bool handle_update_error(const UpdateInfo& info,
                             const QString& message,
                             QWidget* parent,
                             bool quit_after_open);

#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class UpdaterTestAccess;
#endif
};

#endif
