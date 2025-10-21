#include "CategorizationProgressDialog.hpp"

#include "Logger.hpp"
#include "MainApp.hpp"

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QString>

CategorizationProgressDialog::CategorizationProgressDialog(QWidget* parent,
                                                           MainApp* main_app,
                                                           bool show_subcategory_col)
    : QDialog(parent),
      main_app(main_app)
{
    setWindowTitle(tr("Analyzing Files"));
    resize(800, 600);
    setup_ui(show_subcategory_col);
}


void CategorizationProgressDialog::setup_ui(bool /*show_subcategory_col*/)
{
    auto* layout = new QVBoxLayout(this);

    text_view = new QPlainTextEdit(this);
    text_view->setReadOnly(true);
    text_view->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(text_view, 1);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch(1);

    stop_button = new QPushButton(tr("Stop Analysis"), this);
    QIcon stop_icon = QIcon::fromTheme(QStringLiteral("process-stop"));
    if (stop_icon.isNull()) {
        stop_icon = QIcon(style()->standardIcon(QStyle::SP_BrowserStop));
    }
    stop_button->setIcon(stop_icon);
    stop_button->setIconSize(QSize(18, 18));
    button_layout->addWidget(stop_button);

    layout->addLayout(button_layout);

    connect(stop_button, &QPushButton::clicked, this, &CategorizationProgressDialog::request_stop);
}


void CategorizationProgressDialog::show()
{
    QDialog::show();
    text_view->moveCursor(QTextCursor::End);
}


void CategorizationProgressDialog::hide()
{
    QDialog::hide();
}


void CategorizationProgressDialog::append_text(const std::string& text)
{
    if (!text_view) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("Progress dialog text view is null");
        }
        return;
    }

    QString qt_text = QString::fromStdString(text);
    if (!qt_text.endsWith('\n')) {
        qt_text.append('\n');
    }
    text_view->appendPlainText(qt_text);

    QScrollBar* scroll = text_view->verticalScrollBar();
    if (scroll) {
        scroll->setValue(scroll->maximum());
    }
}


void CategorizationProgressDialog::request_stop()
{
    if (!main_app) {
        return;
    }
    main_app->report_progress("[STOP] Cancelling analysis...");
    main_app->request_stop_analysis();
}
