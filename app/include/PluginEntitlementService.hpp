/**
 * @file PluginEntitlementService.hpp
 * @brief Local verification and storage for signed commercial plugin entitlement receipts.
 */
#pragma once

#include <QByteArray>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Trusted Ed25519 public key used to verify entitlement receipts.
 */
struct PluginEntitlementPublicKey {
    /** @brief Stable key id referenced by the signed receipt wrapper. */
    std::string key_id;
    /** @brief Raw 32-byte Ed25519 public key. */
    std::array<unsigned char, 32> ed25519_public_key{};
};

/**
 * @brief Verifies locally cached plugin entitlement receipts.
 *
 * The server remains authoritative. This service only verifies signed receipt
 * payloads issued by the server and answers whether a plugin/product is locally
 * unlocked within the receipt's offline window.
 */
class PluginEntitlementService {
   public:
    /**
     * @brief Constructs a receipt verifier rooted at the app config directory.
     * @param config_dir Base configuration directory used for cached receipts.
     * @param trusted_keys Trusted receipt verification keys. Empty uses compiled-in defaults.
     */
    explicit PluginEntitlementService(std::filesystem::path config_dir,
                                      std::vector<PluginEntitlementPublicKey> trusted_keys = {});

    /**
     * @brief Returns the directory where receipt wrappers are cached.
     * @param config_dir Application config directory.
     * @return Filesystem path for cached entitlement receipts.
     */
    static std::filesystem::path receipt_directory_for_config_dir(const std::filesystem::path& config_dir);

    /**
     * @brief Returns public keys compiled into this app build.
     * @return Trusted public keys for entitlement receipts.
     */
    static std::vector<PluginEntitlementPublicKey> default_public_keys();

    /**
     * @brief Returns a privacy-preserving id for this app installation/device.
     * @return Stable device id used to bind signed entitlement receipts.
     */
    std::string device_id() const;

    /**
     * @brief Stores a server-issued entitlement receipt after verification.
     * @param payload Exact JSON payload bytes signed by the server.
     * @param signature Detached Ed25519 signature over `payload`.
     * @param key_id Trusted key id used by the server signer.
     * @param error Optional output for a user-facing failure reason.
     * @return True when the receipt is valid and was written to local storage.
     */
    bool store_receipt(const QByteArray& payload, const QByteArray& signature, const std::string& key_id,
                       std::string* error = nullptr) const;

    /**
     * @brief Checks whether a signed cached receipt unlocks a plugin product.
     * @param product_id Commercial product id required by the plugin manifest.
     * @param plugin_id Optional concrete plugin id to match inside the receipt.
     * @return True when a valid, active, device-bound receipt grants access.
     */
    bool has_entitlement(std::string_view product_id, std::string_view plugin_id = {}) const;

   private:
    std::vector<PluginEntitlementPublicKey> trusted_keys() const;
    std::filesystem::path receipt_directory() const;

    std::filesystem::path config_dir_;
    std::vector<PluginEntitlementPublicKey> trusted_keys_;
};
