#pragma once

namespace PluginBuildConfig {

/**
 * @brief Returns whether this build uses the Linux repository-friendly plugin profile.
 * @return True when distro repository behavior is compiled in.
 */
constexpr bool distro_repository_build() {
#if defined(AIFS_DISTRO_REPOSITORY_BUILD)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Returns whether remote plugin catalogs and package downloads are enabled.
 * @return True when the app may fetch plugin catalog metadata or catalog-delivered packages.
 */
constexpr bool remote_catalogs_enabled() {
#if defined(AIFS_REMOTE_PLUGIN_CATALOGS_DISABLED)
    return false;
#else
    return true;
#endif
}

/**
 * @brief Returns whether in-app licensed plugin obtain and activation actions are enabled.
 * @return True when purchase links and license activation flows may be shown.
 */
constexpr bool commercial_plugin_actions_enabled() {
#if defined(AIFS_COMMERCIAL_PLUGIN_ACTIONS_DISABLED)
    return false;
#else
    return true;
#endif
}

}  // namespace PluginBuildConfig
