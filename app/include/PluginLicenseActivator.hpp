#pragma once

#include <QJsonObject>
#include <filesystem>
#include <string>

/**
 * @brief Request data used to activate a commercial plugin license.
 */
struct PluginLicenseActivationRequest {
    /** @brief Activation endpoint URL. Empty uses the app default or environment override. */
    std::string activation_url;
    /** @brief User-provided license key. */
    std::string license_key;
    /** @brief Product id requested by the plugin manifest. */
    std::string product_id;
    /** @brief App version reported to the activation service. */
    std::string app_version;
    /** @brief Platform reported to the activation service. */
    std::string platform;
};

/**
 * @brief Result from a plugin license activation attempt.
 */
struct PluginLicenseActivationResult {
    /** @brief True when the license was activated and the signed receipt was stored. */
    bool activated{false};
    /** @brief HTTP status returned by the activation endpoint, when available. */
    long http_status{0};
    /** @brief User-facing status or failure message. */
    std::string message;
    /** @brief Parsed activation response JSON. */
    QJsonObject response;
};

/**
 * @brief Contacts the activation service and stores verified plugin entitlement receipts.
 */
class PluginLicenseActivator {
   public:
    /**
     * @brief Returns the default plugin activation endpoint URL.
     * @return Activation URL, honoring AI_FILE_SORTER_PLUGIN_ACTIVATION_URL when set.
     */
    static std::string default_activation_url();

    /**
     * @brief Returns the normalized current platform key.
     * @return Platform value sent to the activation service.
     */
    static std::string default_platform();

    /**
     * @brief Activates a license and stores the returned signed entitlement receipt.
     * @param config_dir Application config directory where receipts are cached.
     * @param request Activation request data.
     * @return Activation result with response details or a user-facing failure.
     */
    static PluginLicenseActivationResult activate(const std::filesystem::path& config_dir,
                                                  const PluginLicenseActivationRequest& request);
};
