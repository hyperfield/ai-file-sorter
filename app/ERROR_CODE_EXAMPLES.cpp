// Example demonstrating the error code system
// This is documentation - not meant to be compiled

#include "AppException.hpp"
#include "ErrorCode.hpp"
#include "DialogUtils.hpp"

using namespace ErrorCodes;

// Example 1: Throwing an error with automatic message and resolution
void example_network_error() {
    // Throws with code 1000, provides user-friendly message about network issues
    // and includes resolution steps like "Check your network connection", etc.
    throw AppException(Code::NETWORK_UNAVAILABLE);
}

// Example 2: Throwing an error with context
void example_file_error(const std::string& filename) {
    // Throws with code 1200, includes the filename in technical details
    throw AppException(Code::FILE_NOT_FOUND, "File: " + filename);
}

// Example 3: Throwing an error with custom message but standard resolution
void example_api_error(const std::string& details) {
    // Throws with code 1100, uses custom message but keeps resolution steps
    throw AppException(Code::API_AUTHENTICATION_FAILED,
        "Authentication failed: " + details,
        "Server response included: " + details);
}

// Example 4: Catching and displaying errors
void example_catch_and_display(QWidget* parent) {
    try {
        // Some operation that might fail
        throw AppException(Code::LLM_MODEL_LOAD_FAILED, "Model path: /tmp/model.gguf");
    }
    catch (const AppException& ex) {
        // Shows error dialog with:
        // - Error Code: 1401
        // - User-friendly message
        // - Resolution steps
        // - "Copy Error Details" button
        DialogUtils::show_error_dialog(parent, ex);
    }
    catch (const std::exception& ex) {
        // Fallback for non-AppException errors
        DialogUtils::show_error_dialog(parent, ex.what());
    }
}

// Example 5: Using convenience macros
void example_macros() {
    // Shorthand for throwing with context
    THROW_APP_ERROR(Code::DB_CONNECTION_FAILED, "Database: categorization.db");
    
    // Shorthand for throwing with custom message
    THROW_APP_ERROR_MSG(Code::CONFIG_INVALID,
        "Config value out of range: timeout=-1",
        "Config file: settings.ini");
}

// Example 6: Getting error information programmatically
void example_get_error_info() {
    auto info = ErrorCatalog::get_error_info(Code::API_RATE_LIMIT_EXCEEDED);
    
    // Access components:
    int code = static_cast<int>(info.code);  // 1103
    std::string message = info.message;      // "API rate limit exceeded..."
    std::string resolution = info.resolution; // Steps to fix
    std::string user_msg = info.get_user_message();  // Message + resolution
    std::string full = info.get_full_details();      // Everything including code
}

/* Error Code Categories:

Network (1000-1099):
- NETWORK_UNAVAILABLE, CONNECTION_FAILED, TIMEOUT, DNS_RESOLUTION_FAILED,
  SSL_HANDSHAKE_FAILED, SSL_CERTIFICATE_INVALID, PROXY_ERROR

API (1100-1199):
- AUTHENTICATION_FAILED, INVALID_KEY, KEY_MISSING, RATE_LIMIT_EXCEEDED,
  QUOTA_EXCEEDED, INSUFFICIENT_PERMISSIONS, INVALID_REQUEST,
  INVALID_RESPONSE, RESPONSE_PARSE_ERROR, SERVER_ERROR, SERVICE_UNAVAILABLE,
  REQUEST_TIMEOUT, RETRIES_EXHAUSTED

File System (1200-1299):
- FILE_NOT_FOUND, ACCESS_DENIED, PERMISSION_DENIED, ALREADY_EXISTS,
  OPEN_FAILED, READ_FAILED, WRITE_FAILED, DELETE_FAILED, MOVE_FAILED,
  COPY_FAILED, DIRECTORY_NOT_FOUND, DIRECTORY_INVALID, DIRECTORY_ACCESS_DENIED,
  DIRECTORY_CREATE_FAILED, DIRECTORY_NOT_EMPTY, DISK_FULL, DISK_IO_ERROR,
  PATH_INVALID, PATH_TOO_LONG

Database (1300-1399):
- CONNECTION_FAILED, QUERY_FAILED, INIT_FAILED, CORRUPTED, LOCKED,
  CONSTRAINT_VIOLATION, TRANSACTION_FAILED, READONLY

LLM (1400-1499):
- MODEL_NOT_FOUND, MODEL_LOAD_FAILED, MODEL_CORRUPTED, INFERENCE_FAILED,
  CONTEXT_OVERFLOW, INVALID_PROMPT, RESPONSE_EMPTY, RESPONSE_INVALID,
  BACKEND_INIT_FAILED, OUT_OF_MEMORY, TIMEOUT, CLIENT_CREATION_FAILED,
  GPU_NOT_AVAILABLE

Configuration (1500-1599):
- INVALID, MISSING, PARSE_ERROR, SAVE_FAILED, LOAD_FAILED, INVALID_VALUE,
  REQUIRED_FIELD_MISSING

Validation (1600-1699):
- INVALID_INPUT, INVALID_FORMAT, INVALID_CATEGORY, INVALID_SUBCATEGORY,
  EMPTY_FIELD, VALUE_OUT_OF_RANGE

System (1700-1799):
- OUT_OF_MEMORY, UNSUPPORTED_PLATFORM, ENVIRONMENT_VARIABLE_NOT_SET,
  LIBRARY_LOAD_FAILED, INIT_FAILED, RESOURCE_UNAVAILABLE

Categorization (1800-1899):
- NO_FILES, FAILED, PARTIAL_FAILURE, CANCELLED, TIMEOUT

Download (1900-1999):
- FAILED, CURL_INIT_FAILED, INVALID_URL, NETWORK_ERROR, WRITE_ERROR,
  INCOMPLETE

*/
