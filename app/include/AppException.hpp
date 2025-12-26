#ifndef APPEXCEPTION_HPP
#define APPEXCEPTION_HPP

#include "ErrorCode.hpp"
#include <stdexcept>
#include <string>

namespace ErrorCodes {

// Application exception with error code support
class AppException : public std::runtime_error {
public:
    // Constructor with error code and optional context
    explicit AppException(Code code, const std::string& context = "")
        : std::runtime_error(ErrorCatalog::get_error_info(code, context).get_user_message()),
          error_code_(code),
          error_info_(ErrorCatalog::get_error_info(code, context)) {}
    
    // Constructor with error code and custom message (overrides catalog)
    AppException(Code code, const std::string& custom_message, const std::string& context)
        : std::runtime_error(custom_message),
          error_code_(code),
          error_info_(code, custom_message, ErrorCatalog::get_error_info(code).resolution, context) {}
    
    // Get the error code
    Code get_error_code() const noexcept { return error_code_; }
    
    // Get the full error information
    const ErrorInfo& get_error_info() const noexcept { return error_info_; }
    
    // Get user-friendly message
    std::string get_user_message() const { return error_info_.get_user_message(); }
    
    // Get full error details with code and technical info
    std::string get_full_details() const { return error_info_.get_full_details(); }
    
    // Get just the error code as integer
    int get_error_code_int() const noexcept { return static_cast<int>(error_code_); }

private:
    Code error_code_;
    ErrorInfo error_info_;
};

} // namespace ErrorCodes

// Convenience macro for throwing with automatic context
#define THROW_APP_ERROR(code, context) \
    throw ErrorCodes::AppException(code, context)

// Convenience macro for throwing with custom message
#define THROW_APP_ERROR_MSG(code, message, context) \
    throw ErrorCodes::AppException(code, message, context)

#endif // APPEXCEPTION_HPP
