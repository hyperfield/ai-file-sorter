#include "DialogUtils.hpp"
#include "AppException.hpp"
#include "ErrorCode.hpp"

#include <QMessageBox>
#include <QObject>
#include <QString>
#include <QPushButton>
#include <QClipboard>
#include <QApplication>

void DialogUtils::show_error_dialog(QWidget* parent, const std::string& message)
{
    QMessageBox::critical(parent, QObject::tr("Error"), QString::fromStdString(message));
}

void DialogUtils::show_error_dialog(QWidget* parent, ErrorCodes::Code error_code, const std::string& context)
{
    auto error_info = ErrorCodes::ErrorCatalog::get_error_info(error_code, context);
    
    QMessageBox msg_box(parent);
    msg_box.setIcon(QMessageBox::Critical);
    msg_box.setWindowTitle(QObject::tr("Error"));
    msg_box.setText(QString::fromStdString(error_info.get_user_message()));
    
    // Add "Copy Error Details" button
    QPushButton* copy_button = msg_box.addButton(QObject::tr("Copy Error Details"), QMessageBox::ActionRole);
    msg_box.addButton(QMessageBox::Ok);
    
    msg_box.exec();
    
    // If user clicked "Copy Error Details", copy full details to clipboard
    if (msg_box.clickedButton() == copy_button) {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(QString::fromStdString(error_info.get_full_details()));
        
        // Show confirmation
        QMessageBox::information(parent, QObject::tr("Copied"),
            QObject::tr("Error details have been copied to clipboard."));
    }
}

void DialogUtils::show_error_dialog(QWidget* parent, const ErrorCodes::AppException& exception)
{
    const auto& error_info = exception.get_error_info();
    
    QMessageBox msg_box(parent);
    msg_box.setIcon(QMessageBox::Critical);
    msg_box.setWindowTitle(QObject::tr("Error"));
    msg_box.setText(QString::fromStdString(error_info.get_user_message()));
    
    // Add "Copy Error Details" button
    QPushButton* copy_button = msg_box.addButton(QObject::tr("Copy Error Details"), QMessageBox::ActionRole);
    msg_box.addButton(QMessageBox::Ok);
    
    msg_box.exec();
    
    // If user clicked "Copy Error Details", copy full details to clipboard
    if (msg_box.clickedButton() == copy_button) {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(QString::fromStdString(error_info.get_full_details()));
        
        // Show confirmation
        QMessageBox::information(parent, QObject::tr("Copied"),
            QObject::tr("Error details have been copied to clipboard."));
    }
}
