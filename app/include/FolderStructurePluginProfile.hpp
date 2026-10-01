#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Declarative profile contributed by a signed folder-structure plugin.
 */
struct FolderStructurePluginProfile {
    /** @brief Stable profile id inside the plugin package. */
    std::string id;
    /** @brief User-facing profile name. */
    std::string name;
    /** @brief User-facing profile description. */
    std::string description;
    /** @brief Host-recognized strategy key, for example johnny_decimal. */
    std::string structure_kind;
    /** @brief True when the profile can be used to initialize folders. */
    bool available{true};
    /** @brief Detector keys that activate the profile for an existing tree. */
    std::vector<std::string> detectors;
    /** @brief Starter folder paths provided by the profile. */
    std::vector<std::string> initial_directories;
    /** @brief Additional LLM guidance when this profile matches an existing tree. */
    std::string prompt_guidance;
    /** @brief Additional guidance for suggested new folders. */
    std::string new_folder_guidance;
    /** @brief Path to the profile JSON inside the installed package. */
    std::filesystem::path source_path;
};

/**
 * @brief Manifest for a declarative folder-structure plugin package.
 */
struct FolderStructurePluginManifest {
    /** @brief Stable plugin id. */
    std::string id;
    /** @brief User-facing plugin name. */
    std::string name;
    /** @brief User-facing plugin description. */
    std::string description;
    /** @brief Plugin package version. */
    std::string version;
    /** @brief Must be folder_structure_profile for declarative structure plugins. */
    std::string entry_point_kind;
    /** @brief Relative JSON profile paths inside the package. */
    std::vector<std::string> profile_paths;
    /** @brief Optional platform constraints. Empty means any platform. */
    std::vector<std::string> platforms;
    /** @brief Optional architecture constraints. Empty means any architecture. */
    std::vector<std::string> architectures;
    /** @brief Manifest path inside an extracted or installed package. */
    std::filesystem::path source_path;
    /** @brief Trusted key id that verified the package. */
    std::string verified_signer_key_id;
};

/**
 * @brief Trusted Ed25519 public key used to verify folder-structure plugin packages.
 */
struct FolderStructurePluginPublicKey {
    /** @brief Stable key id referenced by plugin-signature.json. */
    std::string key_id;
    /** @brief Raw 32-byte Ed25519 public key. */
    std::array<unsigned char, 32> ed25519_public_key{};
};

/**
 * @brief Returns public keys compiled into this app build.
 * @return Trusted public keys for folder-structure plugin signatures.
 */
std::vector<FolderStructurePluginPublicKey> default_folder_structure_plugin_public_keys();

/**
 * @brief Reports the current platform key used by folder-structure plugin manifests.
 * @return Normalized platform name.
 */
std::string folder_structure_plugin_current_platform();

/**
 * @brief Reports the current CPU architecture key used by folder-structure plugin manifests.
 * @return Normalized architecture name.
 */
std::string folder_structure_plugin_current_architecture();

/**
 * @brief Checks whether a manifest targets the current runtime.
 * @param manifest Manifest to validate.
 * @param error Optional output for a user-facing failure reason.
 * @return True when platform and architecture constraints match.
 */
bool folder_structure_plugin_manifest_matches_current_runtime(
    const FolderStructurePluginManifest& manifest,
    std::string* error = nullptr);

/**
 * @brief Loads and validates a folder-structure plugin manifest file.
 * @param manifest_path Path to manifest.json.
 * @param error Optional output for a user-facing failure reason.
 * @return Parsed manifest when valid.
 */
std::optional<FolderStructurePluginManifest> load_folder_structure_plugin_manifest_from_file(
    const std::filesystem::path& manifest_path,
    std::string* error = nullptr);

/**
 * @brief Loads and validates a declarative structure profile file.
 * @param profile_path Path to a profile JSON file.
 * @param fallback_manifest Manifest used for fallback id/name/description.
 * @param error Optional output for a user-facing failure reason.
 * @return Parsed profile when valid.
 */
std::optional<FolderStructurePluginProfile> load_folder_structure_plugin_profile_from_file(
    const std::filesystem::path& profile_path,
    const FolderStructurePluginManifest& fallback_manifest,
    std::string* error = nullptr);

/**
 * @brief Loads every profile referenced by a manifest.
 * @param manifest Manifest whose profile_paths should be loaded.
 * @param error Optional output for a user-facing failure reason.
 * @return Valid profiles loaded from the package.
 */
std::vector<FolderStructurePluginProfile> load_folder_structure_plugin_profiles(
    const FolderStructurePluginManifest& manifest,
    std::string* error = nullptr);

/**
 * @brief Verifies the detached signature and signed file hashes for a package root.
 * @param package_root Extracted or installed plugin package root.
 * @param trusted_keys Trusted Ed25519 public keys.
 * @param signer_key_id Optional output for the key id that verified the package.
 * @param error Optional output for a user-facing failure reason.
 * @return True when the package is signed and all payload files match.
 */
bool verify_folder_structure_plugin_package(
    const std::filesystem::path& package_root,
    const std::vector<FolderStructurePluginPublicKey>& trusted_keys,
    std::string* signer_key_id = nullptr,
    std::string* error = nullptr);
