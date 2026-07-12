#ifndef APP_ICON_RESOURCES_HPP
#define APP_ICON_RESOURCES_HPP

#include <QIcon>

/**
 * @brief Helpers for loading the application's embedded icon resources.
 */
namespace AppIconResources {

/**
 * @brief Build the multi-resolution application icon from embedded resources.
 * @return A QIcon populated with the embedded icon sizes, or a fallback logo icon.
 */
QIcon build_window_icon();

} // namespace AppIconResources

#endif // APP_ICON_RESOURCES_HPP
