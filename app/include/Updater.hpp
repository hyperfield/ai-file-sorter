#ifndef UPDATER_HPP
#define UPDATER_HPP

#include "Settings.hpp"
#include "Version.hpp"
#include <future>
#include <optional>
#include <string>

class QWidget;

struct UpdateInfo
{
    std::string current_version;
    std::string min_version;
    std::string download_url;
    std::string release_notes_url;
    bool is_required{false};

    UpdateInfo() = default;
    UpdateInfo(const std::string& current_version,
               const std::string& min_version,
               const std::string& download,
               const std::string& notes,
               bool is_required = false)
        : current_version(current_version),
          min_version(min_version),
          download_url(download),
          release_notes_url(notes),
          is_required(is_required) {}
};


class Updater
{
public:
    explicit Updater(Settings& settings);
    ~Updater();
    void begin();

private:
    Settings& settings;
    const std::string update_spec_file_url;
    std::optional<UpdateInfo> update_info;
    std::future<void> update_future;
    void check_updates();
    std::string fetch_update_metadata() const;
    Version string_to_Version(const std::string &version_str);
    void display_update_dialog(bool is_required=false);
    void show_required_update_dialog(const UpdateInfo& info, QWidget* parent) const;
    void show_optional_update_dialog(const UpdateInfo& info, QWidget* parent) const;
    bool is_update_available();
    bool is_update_required();
    bool is_update_skipped();
};

#endif
