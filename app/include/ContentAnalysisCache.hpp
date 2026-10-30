#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

struct sqlite3;

/** @brief Complete content evidence stored in the disposable categorization database. */
class ContentAnalysisCache {
public:
    /** @brief Identity of a source version and the analyzer that produced its evidence. */
    struct Key {
        std::string path;
        std::string analyzer;
        std::string sha256;
    };

    /** @brief Content-derived name and the unabridged description used for sorting. */
    struct Evidence {
        std::string suggested_name;
        std::string description;
    };

    /**
     * @brief Borrow an existing SQLite connection without taking ownership.
     * @param database Connection owned by DatabaseManager, or null when unavailable.
     * @param ensure_paths Refreshes connection-local lookup keys before cache access.
     */
    explicit ContentAnalysisCache(sqlite3* database, std::function<bool()> ensure_paths = {})
        : db_(database), ensure_paths_(std::move(ensure_paths)) {}

    /** @brief Create the versioned evidence table. @return True on success. */
    bool initialize() const;

    /**
     * @brief Fingerprint a readable source before analysis or lookup.
     * @param path Source file, never a generated destination name.
     * @param analyzer Analyzer/model identity, including its prompt version.
     * @return Source identity, or empty if the file cannot be read.
     */
    static std::optional<Key> make_key(const std::filesystem::path& path,
                                       const std::string& analyzer);

    /**
     * @brief Load full evidence for an exact source version and analyzer.
     * @param key Fingerprinted source identity.
     * @return Complete evidence, or empty for missing/legacy/incomplete entries.
     */
    std::optional<Evidence> find(const Key& key) const;

    /**
     * @brief Persist evidence only if the source still matches its pre-analysis hash.
     * @param key Source identity captured before analysis.
     * @param evidence Content-derived filename and description.
     * @return True when complete evidence was stored for an unchanged source.
     */
    bool store(const Key& key, const Evidence& evidence) const;

    /**
     * @brief Remove evidence in a directory when its categorization cache is cleared.
     * @param directory Source directory to clear.
     * @param recursive Include nested source directories when true.
     * @return True when matching evidence was deleted.
     */
    bool clear_directory(const std::string& directory, bool recursive) const;

private:
    sqlite3* db_;
    std::function<bool()> ensure_paths_;
};
