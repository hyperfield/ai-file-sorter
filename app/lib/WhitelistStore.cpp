#include "WhitelistStore.hpp"
#include "Logger.hpp"
#include "Settings.hpp"

#include <QSettings>
#include <algorithm>

namespace {
std::vector<std::string> split_csv(const QString& value) {
    std::vector<std::string> out;
    const auto parts = value.split(",");
    for (const auto& part : parts) {
        QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            out.emplace_back(trimmed.toStdString());
        }
    }
    return out;
}

QString join_csv(const std::vector<std::string>& values) {
    QStringList list;
    for (const auto& v : values) {
        list << QString::fromStdString(v);
    }
    return list.join(", ");
}
}

WhitelistStore::WhitelistStore(std::string config_dir)
    : file_path_(std::move(config_dir) + "/whitelists.ini") {}

bool WhitelistStore::load()
{
    entries_.clear();
    QSettings settings(QString::fromStdString(file_path_), QSettings::IniFormat);
    const QStringList groups = settings.childGroups();
    for (const auto& group : groups) {
        settings.beginGroup(group);
        const auto cats = split_csv(settings.value("Categories").toString());
        const auto subs = split_csv(settings.value("Subcategories").toString());
        settings.endGroup();
        if (!cats.empty() || !subs.empty()) {
            entries_[group.toStdString()] = WhitelistEntry{cats, subs};
        }
    }
    if (entries_.empty()) {
        ensure_default_from_legacy({}, {});
        save();
    }
    return true;
}

bool WhitelistStore::save() const
{
    QSettings settings(QString::fromStdString(file_path_), QSettings::IniFormat);
    settings.clear();
    for (const auto& pair : entries_) {
        settings.beginGroup(QString::fromStdString(pair.first));
        settings.setValue("Categories", join_csv(pair.second.categories));
        settings.setValue("Subcategories", join_csv(pair.second.subcategories));
        settings.endGroup();
    }
    settings.sync();
    return settings.status() == QSettings::NoError;
}

std::vector<std::string> WhitelistStore::list_names() const
{
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto& entry : entries_) {
        names.push_back(entry.first);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::optional<WhitelistEntry> WhitelistStore::get(const std::string& name) const
{
    if (auto it = entries_.find(name); it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void WhitelistStore::set(const std::string& name, WhitelistEntry entry)
{
    entries_[name] = std::move(entry);
}

void WhitelistStore::remove(const std::string& name)
{
    entries_.erase(name);
}

void WhitelistStore::ensure_default_from_legacy(const std::vector<std::string>& cats,
                                                const std::vector<std::string>& subs)
{
    if (!entries_.empty()) {
        return;
    }
    std::vector<std::string> use_cats = cats;
    std::vector<std::string> use_subs = subs;
    if (use_cats.empty()) {
        use_cats = {
            "Archives", "Backups", "Books", "Configs", "Data Exports",
            "Development", "Documents", "Drivers", "Ebooks", "Firmware",
            "Guides", "Images", "Installers", "Licenses", "Manuals",
            "Music", "Operating Systems", "Presentations", "Software", "Spreadsheets", "System",
            "Temporary", "Videos"
        };
    }
    if (use_subs.empty()) {
        use_subs = {};
    }
    entries_[default_name_] = WhitelistEntry{use_cats, use_subs};
}

void WhitelistStore::initialize_from_settings(Settings& settings)
{
    load();
    ensure_default_from_legacy(settings.get_allowed_categories(),
                               settings.get_allowed_subcategories());
    save();

    if (settings.get_active_whitelist().empty()) {
        settings.set_active_whitelist(default_name_);
    }

    auto active = settings.get_active_whitelist();
    if (auto entry = get(active)) {
        settings.set_allowed_categories(entry->categories);
        settings.set_allowed_subcategories(entry->subcategories);
    }
}
