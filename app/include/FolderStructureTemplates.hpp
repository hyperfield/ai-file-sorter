#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct FolderStructurePluginProfile;

namespace FolderStructureTemplates {

/**
 * @brief Built-in starter folder structure identifiers.
 */
enum class Id {
    JohnnyDecimal,
    Para,
    Gtd,
    Chronological,
    ClientProjectDeliverable,
    DepartmentFunction,
    MediaType,
    StatusLifecycle,
    KnowledgeBase,
    NumberedPrefix
};

/**
 * @brief User-facing metadata and folder paths for a starter structure.
 */
struct Descriptor {
    Id id{Id::JohnnyDecimal};
    std::string plugin_profile_id;
    std::string name;
    std::string description;
    bool available{true};
    std::vector<std::string> relative_directories;
};

/**
 * @brief Result of creating a starter structure on disk.
 */
struct CreationResult {
    bool success{false};
    std::vector<std::filesystem::path> created_directories;
    std::vector<std::filesystem::path> existing_directories;
    std::string error;
};

/**
 * @brief Return every built-in starter folder structure descriptor.
 * @return Stable list of supported and planned templates.
 */
const std::vector<Descriptor>& all();

/**
 * @brief Return built-in templates followed by installed plugin templates.
 * @param plugin_profiles Declarative profiles loaded from verified plugins.
 * @return Template descriptors usable by the initializer dialog.
 */
std::vector<Descriptor> all_with_plugins(
    const std::vector<FolderStructurePluginProfile>& plugin_profiles);

/**
 * @brief Find one starter folder structure descriptor.
 * @param id Template identifier to find.
 * @return Pointer to the descriptor, or nullptr when no template matches.
 */
const Descriptor* find(Id id);

/**
 * @brief Create a starter folder structure below a destination root from a descriptor.
 * @param root Destination folder where the structure should be initialized.
 * @param descriptor Descriptor to create.
 * @return Creation result with created/existing folder paths or an error.
 */
CreationResult create(const std::filesystem::path& root, const Descriptor& descriptor);

/**
 * @brief Create a starter folder structure below a destination root.
 * @param root Destination folder where the structure should be initialized.
 * @param id Template identifier to create.
 * @return Creation result with created/existing folder paths or an error.
 */
CreationResult create(const std::filesystem::path& root, Id id);

}  // namespace FolderStructureTemplates
