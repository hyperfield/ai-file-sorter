#include "WhitelistStore.hpp"
#include "Logger.hpp"
#include "Settings.hpp"

#include <QSettings>
#include <algorithm>
#include <cctype>

namespace {
constexpr char kMetadataGroup[] = "__meta__";
constexpr char kBuiltInSeedVersionKey[] = "BuiltInSeedVersion";
constexpr int kCurrentBuiltInSeedVersion = 4;
constexpr char kDocumentsWhitelistName[] = "Documents";
constexpr char kLegacyMusicCategory[] = "music";
constexpr char kCanonicalAudioCategory[] = "Audio";

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool migrate_legacy_category_names(std::vector<std::string>& categories) {
    std::vector<std::string> migrated;
    migrated.reserve(categories.size());

    bool changed = false;
    for (const auto& category : categories) {
        std::string canonical = category;
        if (to_lower_copy(category) == kLegacyMusicCategory) {
            canonical = kCanonicalAudioCategory;
            changed = true;
        }

        if (std::find(migrated.begin(), migrated.end(), canonical) == migrated.end()) {
            migrated.push_back(canonical);
        } else if (canonical != category) {
            changed = true;
        }
    }

    if (changed) {
        categories = std::move(migrated);
    }
    return changed;
}

bool migrate_legacy_category_map_names(
    std::unordered_map<std::string, std::vector<std::string>>& subcategories_by_category)
{
    std::unordered_map<std::string, std::vector<std::string>> migrated;
    bool changed = false;
    for (const auto& [category, subcategories] : subcategories_by_category) {
        const std::string canonical =
            to_lower_copy(category) == kLegacyMusicCategory ? kCanonicalAudioCategory : category;
        changed = changed || canonical != category;
        auto& target = migrated[canonical];
        for (const auto& subcategory : subcategories) {
            if (std::find(target.begin(), target.end(), subcategory) == target.end()) {
                target.push_back(subcategory);
            }
        }
    }

    if (changed) {
        subcategories_by_category = std::move(migrated);
    }
    return changed;
}

std::vector<std::string> split_csv(const QString& value) {
    std::vector<std::string> out;
    const QChar delimiter = value.contains(';') ? QChar(';') : QChar(',');
    const auto parts = value.split(delimiter);
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

std::vector<std::string> document_topic_subcategories()
{
    return {
        "Invoices", "Receipts", "Taxes", "Contracts", "Reports", "Statements",
        "Letters", "Forms", "Certificates", "Policies", "Manuals", "Notes",
        "Presentations", "Spreadsheets", "Legal", "Insurance", "Banking"
    };
}

std::unordered_map<std::string, std::vector<std::string>> read_subcategories_by_category(
    QSettings& settings)
{
    std::unordered_map<std::string, std::vector<std::string>> result;
    const int size = settings.beginReadArray(QStringLiteral("SubcategoriesByCategory"));
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        const auto category = settings.value(QStringLiteral("Category")).toString().trimmed();
        if (category.isEmpty()) {
            continue;
        }
        auto subcategories = split_csv(settings.value(QStringLiteral("Subcategories")).toString());
        if (!subcategories.empty()) {
            result[category.toStdString()] = std::move(subcategories);
        }
    }
    settings.endArray();
    return result;
}

void write_subcategories_by_category(
    QSettings& settings,
    const std::unordered_map<std::string, std::vector<std::string>>& subcategories_by_category)
{
    std::vector<std::string> categories;
    categories.reserve(subcategories_by_category.size());
    for (const auto& [category, subcategories] : subcategories_by_category) {
        if (!category.empty() && !subcategories.empty()) {
            categories.push_back(category);
        }
    }
    std::sort(categories.begin(), categories.end());

    settings.beginWriteArray(QStringLiteral("SubcategoriesByCategory"),
                             static_cast<int>(categories.size()));
    for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
        const auto& category = categories[static_cast<std::size_t>(i)];
        const auto it = subcategories_by_category.find(category);
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("Category"), QString::fromStdString(category));
        settings.setValue(QStringLiteral("Subcategories"), join_csv(it->second));
    }
    settings.endArray();
}

WhitelistEntry make_documents_entry()
{
    auto subcategories = document_topic_subcategories();
    return WhitelistEntry{
        {"Documents"},
        {},
        {{"Documents", std::move(subcategories)}}
    };
}

bool is_legacy_documents_entry(const WhitelistEntry& entry)
{
    return entry.subcategories.empty() &&
           entry.subcategories_by_category.empty() &&
           entry.categories == document_topic_subcategories();
}
}

WhitelistStore::WhitelistStore(std::string config_dir)
    : file_path_(std::move(config_dir) + "/whitelists.ini") {}

bool WhitelistStore::load()
{
    entries_.clear();
    QSettings settings(QString::fromStdString(file_path_), QSettings::IniFormat);
    built_in_seed_version_ = settings.value(QString::fromLatin1("%1/%2")
                                                .arg(QString::fromLatin1(kMetadataGroup),
                                                     QString::fromLatin1(kBuiltInSeedVersionKey)),
                                            0)
                                 .toInt();
    bool changed = built_in_seed_version_ < kCurrentBuiltInSeedVersion;
    const QStringList groups = settings.childGroups();
    for (const auto& group : groups) {
        if (group == QString::fromLatin1(kMetadataGroup)) {
            continue;
        }
        settings.beginGroup(group);
        auto cats = split_csv(settings.value("Categories").toString());
        const auto subs = split_csv(settings.value("Subcategories").toString());
        auto subcategories_by_category = read_subcategories_by_category(settings);
        settings.endGroup();
        changed = migrate_legacy_category_names(cats) || changed;
        changed = migrate_legacy_category_map_names(subcategories_by_category) || changed;
        if (!cats.empty() || !subs.empty() || !subcategories_by_category.empty()) {
            entries_[group.toStdString()] =
                WhitelistEntry{cats, subs, std::move(subcategories_by_category)};
        }
    }
    if (entries_.empty()) {
        ensure_default_from_legacy({}, {});
        changed = true;
    } else if (built_in_seed_version_ < kCurrentBuiltInSeedVersion) {
        auto documents = entries_.find(kDocumentsWhitelistName);
        if (documents == entries_.end()) {
            entries_.emplace(kDocumentsWhitelistName, make_documents_entry());
        } else if (is_legacy_documents_entry(documents->second)) {
            documents->second = make_documents_entry();
        }
    }
    if (changed) {
        save();
    }
    return true;
}

bool WhitelistStore::save() const
{
    QSettings settings(QString::fromStdString(file_path_), QSettings::IniFormat);
    settings.clear();
    settings.beginGroup(QString::fromLatin1(kMetadataGroup));
    settings.setValue(QString::fromLatin1(kBuiltInSeedVersionKey), kCurrentBuiltInSeedVersion);
    settings.endGroup();
    for (const auto& pair : entries_) {
        settings.beginGroup(QString::fromStdString(pair.first));
        settings.setValue("Categories", join_csv(pair.second.categories));
        settings.setValue("Subcategories", join_csv(pair.second.subcategories));
        write_subcategories_by_category(settings, pair.second.subcategories_by_category);
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
            "Audio", "Operating Systems", "Presentations", "Software", "Spreadsheets", "System",
            "Temporary", "Videos"
        };
    }
    migrate_legacy_category_names(use_cats);
    if (use_subs.empty()) {
        use_subs = {};
    }
    entries_[default_name_] = WhitelistEntry{use_cats, use_subs, {}};
    entries_[kDocumentsWhitelistName] = make_documents_entry();
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
        settings.set_allowed_subcategories_by_category(entry->subcategories_by_category);
    }
}
