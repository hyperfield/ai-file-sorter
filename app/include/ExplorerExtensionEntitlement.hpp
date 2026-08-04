/**
 * @file ExplorerExtensionEntitlement.hpp
 * @brief Detects paid Windows Explorer extension entitlement state.
 */
#pragma once

/**
 * @brief Provides app-side detection for paid Explorer extension ownership.
 */
class ExplorerExtensionEntitlement {
public:
    /**
     * @brief Returns whether a valid paid Explorer extension entitlement is present.
     * @return True when the paid extension marker is present and usable.
     */
    static bool has_paid_entitlement();
};
