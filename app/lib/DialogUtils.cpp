#include "DialogUtils.hpp"

#include <QMessageBox>
#include <QObject>
#include <QString>

void DialogUtils::show_error_dialog(QWidget* parent, const std::string& message)
{
    QMessageBox::critical(parent, QObject::tr("Error"), QString::fromStdString(message));
}
