/**
 * @file ErrorMessages.hpp
 * @brief Translatable error message constants used throughout the UI.
 */
#ifndef ERRORMESSAGES_H
#define ERRORMESSAGES_H

#include <libintl.h>

/**
 * @brief Translate a string using gettext.
 * @param String String literal to translate.
 * @return Translated string (const char* managed by gettext).
 */
#define _(String) gettext(String)

/**
 * @brief Error message shown when no files are available for categorization.
 */
#define ERR_NO_FILES_TO_CATEGORIZE _("There are no files or directories to categorize.")
/**
 * @brief Error message shown when the user provides an invalid path.
 */
#define ERR_INVALID_PATH _("Invalid directory path.")
/**
 * @brief Error message shown when no internet connection is detected.
 */
#define ERR_NO_INTERNET_CONNECTION _("No internet connection. Please check your network and try again.")

#endif
