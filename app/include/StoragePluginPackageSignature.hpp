#pragma once

#include <array>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

/**
 * @brief Trusted Ed25519 public key used to verify storage plugin packages.
 */
struct StoragePluginPackagePublicKey {
    /** @brief Stable key id referenced by plugin-signature.json. */
    std::string key_id;
    /** @brief Raw 32-byte Ed25519 public key. */
    std::array<unsigned char, 32> ed25519_public_key{};
};

/**
 * @brief Result metadata from a verified storage plugin package.
 */
struct StoragePluginPackageVerification {
    /** @brief Trusted key id that verified the package signature. */
    std::string signer_key_id;
    /** @brief Normalized package payload paths covered by the signed hash list. */
    std::set<std::string> signed_payload_paths;
};

/**
 * @brief Returns public keys compiled into this app build.
 * @return Trusted public keys for storage plugin package signatures.
 */
std::vector<StoragePluginPackagePublicKey> default_storage_plugin_package_public_keys();

/**
 * @brief Verifies the detached signature and signed file hashes for a storage package root.
 * @param package_root Extracted plugin package root.
 * @param trusted_keys Trusted Ed25519 public keys.
 * @param verification Optional output for signer and signed payload metadata.
 * @param error Optional output for a user-facing failure reason.
 * @return True when the package is signed by a trusted key and all payload hashes match.
 */
bool verify_storage_plugin_package(const std::filesystem::path& package_root,
                                   const std::vector<StoragePluginPackagePublicKey>& trusted_keys,
                                   StoragePluginPackageVerification* verification = nullptr,
                                   std::string* error = nullptr);
