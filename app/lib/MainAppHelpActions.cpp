#include "MainAppHelpActions.hpp"

#include <app_version.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPixmap>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QString>
#include <QDesktopServices>
#include <QUrl>

void MainAppHelpActions::show_about(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("About AI File Sorter"));
    dialog.resize(600, 420);

    auto* layout = new QVBoxLayout(&dialog);
    auto* tabs = new QTabWidget(&dialog);
    layout->addWidget(tabs);

    // About tab
    auto* about_tab = new QWidget(&dialog);
    auto* about_layout = new QVBoxLayout(about_tab);
    about_layout->setSpacing(8);

    if (QPixmap logo_pix(QStringLiteral(":/net/quicknode/AIFileSorter/images/logo.png")); !logo_pix.isNull()) {
        auto* logo_label = new QLabel(about_tab);
        logo_label->setAlignment(Qt::AlignHCenter);
        logo_label->setPixmap(logo_pix.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        about_layout->addWidget(logo_label);
    }

    auto* program_name = new QLabel(QStringLiteral("<h2>AI File Sorter</h2>"), about_tab);
    program_name->setAlignment(Qt::AlignHCenter);
    about_layout->addWidget(program_name);

    const QString version_text = QStringLiteral("Version: %1").arg(QString::fromStdString(APP_VERSION.to_string()));
    auto* version_label = new QLabel(version_text, about_tab);
    version_label->setAlignment(Qt::AlignHCenter);
    about_layout->addWidget(version_label);

    auto* copyright_label =
        new QLabel(QStringLiteral("© 2024-2026 QuickNode. All rights reserved."), about_tab);
    copyright_label->setAlignment(Qt::AlignHCenter);
    about_layout->addWidget(copyright_label);

    auto* website_label = new QLabel(QStringLiteral(
        "<a href=\"https://www.filesorter.app\">Visit the Website</a>"), about_tab);
    website_label->setOpenExternalLinks(true);
    website_label->setAlignment(Qt::AlignHCenter);
    about_layout->addWidget(website_label);

    about_layout->addStretch(1);
    tabs->addTab(about_tab, QObject::tr("About"));

    // Credits tab
    auto* credits_tab = new QWidget(&dialog);
    auto* credits_layout = new QVBoxLayout(credits_tab);
    credits_layout->setSpacing(8);

    if (QPixmap qn_logo_pix(QStringLiteral(":/net/quicknode/AIFileSorter/images/qn_logo.png")); !qn_logo_pix.isNull()) {
        auto* qn_logo = new QLabel(credits_tab);
        qn_logo->setAlignment(Qt::AlignHCenter);
        qn_logo->setPixmap(qn_logo_pix.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        credits_layout->addWidget(qn_logo);
    }

    auto* author_label = new QLabel(QStringLiteral("Author: hyperfield"), credits_tab);
    author_label->setAlignment(Qt::AlignHCenter);
    credits_layout->addWidget(author_label);

    auto* author_details = new QLabel(QStringLiteral(
        "Author's brand name is <a href=\"https://quicknode.net\">QN (QuickNode)</a>.<br>"
        "Source code on GitHub is <a href=\"https://github.com/hyperfield/ai-file-sorter\">here</a>."), credits_tab);
    author_details->setOpenExternalLinks(true);
    author_details->setAlignment(Qt::AlignHCenter);
    author_details->setWordWrap(true);
    credits_layout->addWidget(author_details);

    credits_layout->addStretch(1);
    tabs->addTab(credits_tab, QObject::tr("Credits"));

    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(button_box);

    dialog.exec();
}

void MainAppHelpActions::show_agpl_info(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("About the AGPL License"));
    dialog.resize(520, 320);

    auto* layout = new QVBoxLayout(&dialog);

    auto* summary = new QLabel(QObject::tr(
        "AI File Sorter is distributed under the GNU Affero General Public License v3.0."
        "<br><br>"
        "You can access the full source code at "
        "<a href=\"https://github.com/hyperfield/ai-file-sorter\">github.com/hyperfield/ai-file-sorter</a>."
        "<br><br>"
        "A full copy of the license is provided with this application and available online at "
        "<a href=\"https://www.gnu.org/licenses/agpl-3.0.html\">gnu.org</a>."), &dialog);
    summary->setTextFormat(Qt::RichText);
    summary->setOpenExternalLinks(true);
    summary->setWordWrap(true);
    layout->addWidget(summary);

    layout->addStretch(1);

    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(button_box);

    dialog.exec();
}

void MainAppHelpActions::open_support_page()
{
    const QUrl donation_url(QStringLiteral("https://www.paypal.com/donate/?hosted_button_id=Z3XYTG38C62HQ"));
    QDesktopServices::openUrl(donation_url);
}
