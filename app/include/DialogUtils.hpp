#ifndef DIALOGUTILS_HPP
#define DIALOGUTILS_HPP

#include <string>

class QWidget;

namespace ErrorCodes {
    class AppException;
    enum class Code;
}

class DialogUtils {
public:
    // Show simple error dialog with message
    static void show_error_dialog(QWidget* parent, const std::string& message);
    
    // Show error dialog with error code and full details
    static void show_error_dialog(QWidget* parent, ErrorCodes::Code error_code, const std::string& context = "");
    
    // Show error dialog from AppException
    static void show_error_dialog(QWidget* parent, const ErrorCodes::AppException& exception);
};

#endif // DIALOGUTILS_HPP
