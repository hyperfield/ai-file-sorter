#ifndef HEADLESS_SETTINGS_OVERRIDES_JSON_HPP
#define HEADLESS_SETTINGS_OVERRIDES_JSON_HPP

#include "HeadlessSettingsOverrides.hpp"

#include <QJsonObject>

namespace HeadlessSettingsOverridesJson {

/**
 * @brief Parse headless settings overrides from a JSON object.
 * @param object JSON object using the headless settings override contract.
 * @return Parsed overrides; absent or type-mismatched fields are left unset.
 */
HeadlessSettingsOverrides from_json(const QJsonObject& object);

} // namespace HeadlessSettingsOverridesJson

#endif // HEADLESS_SETTINGS_OVERRIDES_JSON_HPP
