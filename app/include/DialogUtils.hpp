#ifndef DIALOGUTILS_HPP
#define DIALOGUTILS_HPP

#include <string>

class QWidget;

class DialogUtils {
public:
    static void show_error_dialog(QWidget* parent, const std::string& message);
};

#endif // DIALOGUTILS_HPP
