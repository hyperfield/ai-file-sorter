#include <catch2/catch_test_macros.hpp>

#include "StoragePluginDialog.hpp"
#include "StoragePluginManager.hpp"
#include "StoragePluginManifest.hpp"
#include "TestHelpers.hpp"

#include <QCoreApplication>
#include <QPushButton>
#include <QTreeWidget>

#include <fmt/format.h>

#include <filesystem>
#include <fstream>

namespace {

#ifndef AIFS_STORAGE_PLUGIN_STUB_NAME
#define AIFS_STORAGE_PLUGIN_STUB_NAME "aifs_storage_plugin_stub"
#endif

std::filesystem::path storage_plugin_stub_path()
{
    return std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) /
           AIFS_STORAGE_PLUGIN_STUB_NAME;
}

} // namespace

TEST_CASE("StoragePluginDialog shows check for updates button and per-plugin update actions")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    EnvVarGuard catalog_guard("AI_FILE_SORTER_STORAGE_PLUGIN_CATALOG_URL",
                              std::string("https://plugins.example.invalid/storage/catalog.json"));
    QtAppContext qt;
    TempDir config_dir;

    const auto manifest_dir =
        StoragePluginManager::manifest_directory_for_config_dir(config_dir.path().string());
    std::filesystem::create_directories(manifest_dir);

    const auto stub_path = storage_plugin_stub_path();
    REQUIRE(std::filesystem::exists(stub_path));

    const StoragePluginManifest local_manifest{
        .id = "mockcloud_support",
        .name = "MockCloud Storage Support",
        .description = "Locally installed plugin used to test dialog update actions.",
        .version = "1.0.0",
        .provider_ids = {"mockcloud"},
        .entry_point_kind = "external_process",
        .entry_point = stub_path.string()
    };
    REQUIRE(save_storage_plugin_manifest_to_file(local_manifest,
                                                 manifest_dir / "mockcloud_support.json"));

    const auto current_platform = storage_plugin_current_platform();
    const auto current_architecture = storage_plugin_current_architecture();
    const std::string catalog_url = "https://plugins.example.invalid/storage/catalog.json";
    const std::string catalog_json = fmt::format(R"json({{
  "plugins": [
    {{
      "id": "mockcloud_support",
      "name": "MockCloud Storage Support",
      "description": "Catalog-delivered update for the installed mock plugin.",
      "version": "2.0.0",
      "provider_ids": ["mockcloud"],
      "platforms": ["{}"],
      "architectures": ["{}"],
      "remote_manifest_url": "https://plugins.example.invalid/storage/mockcloud/manifest.json",
      "entry_point_kind": "external_process",
      "entry_point": "mockcloud_plugin"
    }}
  ]
}})json",
                                               current_platform,
                                               current_architecture);

    auto download_fn = [catalog_url, catalog_json](const std::string& url,
                                                   const std::filesystem::path& destination,
                                                   StoragePluginPackageFetcher::ProgressCallback,
                                                   StoragePluginPackageFetcher::CancelCheck) {
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream out(destination, std::ios::binary | std::ios::trunc);
        if (url == catalog_url) {
            out << catalog_json;
            return;
        }
        throw std::runtime_error("Unexpected remote plugin URL");
    };

    StoragePluginManager manager(config_dir.path().string(), download_fn);
    REQUIRE(manager.install("mockcloud_support"));
    REQUIRE(manager.refresh_remote_catalog());
    REQUIRE(manager.can_update("mockcloud_support"));

    StoragePluginDialog dialog(manager);

    auto* plugin_list = dialog.findChild<QTreeWidget*>();
    REQUIRE(plugin_list != nullptr);
    REQUIRE(plugin_list->columnCount() == 2);

    QPushButton* check_updates_button = nullptr;
    for (auto* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Check for updates")) {
            check_updates_button = button;
            break;
        }
    }
    REQUIRE(check_updates_button != nullptr);

    QTreeWidgetItem* target_item = nullptr;
    for (int row = 0; row < plugin_list->topLevelItemCount(); ++row) {
        auto* item = plugin_list->topLevelItem(row);
        if (item && item->data(0, Qt::UserRole).toString() == QStringLiteral("mockcloud_support")) {
            target_item = item;
            break;
        }
    }
    REQUIRE(target_item != nullptr);

    QWidget* action_widget = plugin_list->itemWidget(target_item, 1);
    REQUIRE(action_widget != nullptr);

    QPushButton* update_button = nullptr;
    for (auto* button : action_widget->findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Update")) {
            update_button = button;
            break;
        }
    }
    REQUIRE(update_button != nullptr);
}
