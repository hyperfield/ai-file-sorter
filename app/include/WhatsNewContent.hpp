#ifndef WHATS_NEW_CONTENT_HPP
#define WHATS_NEW_CONTENT_HPP

#include "Language.hpp"

#include <QString>

namespace WhatsNewContent {

/**
 * @brief Loads packaged Markdown release notes for an application version.
 * @param version Numeric application version, such as "1.9.0".
 * @return Markdown text for the version, or an empty string when no notes are packaged.
 */
QString markdown_for_version(const QString& version);

/**
 * @brief Loads packaged Markdown release notes for an application version and language.
 * @param version Numeric application version, such as "1.9.0".
 * @param language Preferred content language.
 * @return Localized Markdown text for the version, English Markdown when the translation is
 * unavailable, or an empty string when no notes are packaged.
 */
QString markdown_for_version(const QString& version, Language language);

} // namespace WhatsNewContent

#endif // WHATS_NEW_CONTENT_HPP
