#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "UpdateFeed.hpp"

#include <QString>
#include <functional>

class QWidget;
class Updater;

class UpdaterTestAccess {
public:
    static bool is_update_available(Updater& updater);
    static std::optional<UpdateInfo> current_update_info(const Updater& updater);
    static void set_open_download_url_handler(Updater& updater,
                                              std::function<void(const std::string&)> handler);
    static void set_quit_handler(Updater& updater,
                                 std::function<void()> handler);
    static bool handle_update_error(Updater& updater,
                                    const UpdateInfo& info,
                                    const QString& message,
                                    QWidget* parent,
                                    bool quit_after_open);
};

#endif // AI_FILE_SORTER_TEST_BUILD
