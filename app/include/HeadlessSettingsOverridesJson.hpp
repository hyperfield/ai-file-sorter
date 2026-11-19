#ifndef HEADLESS_SETTINGS_OVERRIDES_JSON_HPP
#define HEADLESS_SETTINGS_OVERRIDES_JSON_HPP

#include "HeadlessSettingsOverrides.hpp"

#include <QJsonObject>

#include <string_view>

namespace HeadlessSettingsOverridesJson {

/**
 * @brief Parse headless settings overrides from a JSON object.
 * @param object JSON object using the headless settings override contract.
 * @return Parsed overrides; absent or type-mismatched fields are left unset.
 */
HeadlessSettingsOverrides from_json(const QJsonObject& object);

/**
 * @brief Parses a job settings sidecar with compatibility for legacy Explorer callers.
 * @param object JSON object using the headless settings override contract.
 * @param job_id Caller job ID; Explorer job IDs start with `explorer-`.
 * @return Parsed overrides. Explorer jobs omitting the sortingMode property use
 * generated categories; explicit properties retain the normal parser behavior.
 */
HeadlessSettingsOverrides from_json_for_job(const QJsonObject& object, std::string_view job_id);

} // namespace HeadlessSettingsOverridesJson

#endif // HEADLESS_SETTINGS_OVERRIDES_JSON_HPP
