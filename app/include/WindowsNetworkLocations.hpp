#ifndef WINDOWS_NETWORK_LOCATIONS_HPP
#define WINDOWS_NETWORK_LOCATIONS_HPP

#include <string>
#include <vector>

class Settings;

/**
 * @brief Describes a Windows network location shown in the file explorer pane.
 */
struct WindowsNetworkLocation {
    std::string label;
    std::string path;
};

/**
 * @brief Discovers bounded Windows network locations without enumerating the full network.
 */
class WindowsNetworkLocations {
public:
    /**
     * @brief Finds mapped network drives, saved network shortcuts, and remembered UNC shares.
     * @param settings App settings used for remembered UNC locations.
     * @return Network locations in display order.
     */
    static std::vector<WindowsNetworkLocation> discover(const Settings& settings);

    /**
     * @brief Returns whether a path is a UNC path.
     * @param path Path text to inspect.
     * @return True when the path begins with a UNC server/share prefix.
     */
    static bool is_unc_path(const std::string& path);

    /**
     * @brief Returns whether a path is a mapped network drive root.
     * @param path Path text to inspect.
     * @return True when the path is a remote Windows drive root.
     */
    static bool is_remote_drive_root(const std::string& path);

    /**
     * @brief Returns whether a path identifies a supported network location.
     * @param path Path text to inspect.
     * @return True for UNC paths and mapped network drive roots.
     */
    static bool is_network_location_path(const std::string& path);

    /**
     * @brief Extracts the UNC share root from a path.
     * @param path UNC path, potentially including child directories.
     * @return Share root such as `\\server\share`, or empty when unavailable.
     */
    static std::string unc_share_root(const std::string& path);
};

#endif // WINDOWS_NETWORK_LOCATIONS_HPP
