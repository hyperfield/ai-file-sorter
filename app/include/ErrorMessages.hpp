#ifndef ERRORMESSAGES_H
#define ERRORMESSAGES_H

#include <libintl.h>
#include "ErrorCode.hpp"

#define _(String) gettext(String)

// Legacy error message macros - kept for backward compatibility
// New code should use ErrorCode enum instead
#define ERR_NO_FILES_TO_CATEGORIZE _("There are no files or directories to categorize.")
#define ERR_INVALID_PATH _("Invalid directory path.")
#define ERR_NO_INTERNET_CONNECTION _("No internet connection. Please check your network and try again.")

// Helper to convert legacy macros to error codes
namespace ErrorMessages {
    inline ErrorCodes::Code get_code_for_message(const char* message) {
        if (message == ERR_NO_FILES_TO_CATEGORIZE) {
            return ErrorCodes::Code::CATEGORIZATION_NO_FILES;
        } else if (message == ERR_INVALID_PATH) {
            return ErrorCodes::Code::PATH_INVALID;
        } else if (message == ERR_NO_INTERNET_CONNECTION) {
            return ErrorCodes::Code::NETWORK_UNAVAILABLE;
        }
        return ErrorCodes::Code::UNKNOWN_ERROR;
    }
}

#endif