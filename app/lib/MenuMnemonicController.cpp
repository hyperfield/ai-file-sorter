#include "MenuMnemonicController.hpp"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QVariant>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>

namespace {

constexpr const char* kMnemonicTitleProperty = "_aifsMnemonicTitle";
constexpr const char* kMnemonicLifecycleConnectedProperty = "_aifsMnemonicLifecycleConnected";

void apply_title_visibility(QMenu* menu, bool visible)
{
    if (!menu) {
        return;
    }

    const QString title = MenuMnemonicController::mnemonic_title(menu);
    menu->setTitle(visible ? title : MenuMnemonicController::strip_mnemonic_markers(title));
}

bool is_alt_key_event(const QEvent* event)
{
    if (!event ||
        (event->type() != QEvent::KeyPress &&
         event->type() != QEvent::KeyRelease &&
         event->type() != QEvent::ShortcutOverride)) {
        return false;
    }

    const auto* key_event = static_cast<const QKeyEvent*>(event);
    return key_event->key() == Qt::Key_Alt && !key_event->isAutoRepeat();
}

} // namespace

MenuMnemonicController::MenuMnemonicController(QMenuBar* menu_bar, QObject* parent)
    : QObject(parent),
      menu_bar_(menu_bar)
{
    if (qApp) {
        qApp->installEventFilter(this);
    }
    refresh_titles();
}

MenuMnemonicController::~MenuMnemonicController()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void MenuMnemonicController::set_mnemonic_title(QMenu* menu, const QString& title)
{
    if (!menu) {
        return;
    }
    menu->setProperty(kMnemonicTitleProperty, title);
    apply_title_visibility(menu, false);
}

QString MenuMnemonicController::mnemonic_title(const QMenu* menu)
{
    if (!menu) {
        return {};
    }

    const QVariant stored = menu->property(kMnemonicTitleProperty);
    if (stored.isValid()) {
        return stored.toString();
    }
    return menu->title();
}

QString MenuMnemonicController::strip_mnemonic_markers(const QString& value)
{
    QString result;
    result.reserve(value.size());
    for (int i = 0; i < value.size(); ++i) {
        const QChar ch = value.at(i);
        if (ch != QChar('&')) {
            result.push_back(ch);
            continue;
        }
        if (i + 1 < value.size() && value.at(i + 1) == QChar('&')) {
            result.push_back(QChar('&'));
            ++i;
        }
    }
    return result;
}

void MenuMnemonicController::refresh_titles()
{
    for (QMenu* const menu : top_level_menus()) {
        connect_menu_lifecycle(menu);
        apply_title_visibility(menu, mnemonics_visible_);
    }
}

void MenuMnemonicController::set_mnemonics_visible(bool visible)
{
    if (mnemonics_visible_ == visible) {
        return;
    }

    mnemonics_visible_ = visible;
    refresh_titles();
}

bool MenuMnemonicController::eventFilter(QObject*, QEvent* event)
{
    if (!event) {
        return false;
    }

    if (event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::ApplicationDeactivate) {
        set_mnemonics_visible(false);
        return false;
    }

    if (is_alt_key_event(event)) {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
            set_mnemonics_visible(true);
        } else if (event->type() == QEvent::KeyRelease) {
            hide_when_menu_navigation_finishes();
        }
    }

    return false;
}

void MenuMnemonicController::connect_menu_lifecycle(QMenu* menu)
{
    if (!menu || menu->property(kMnemonicLifecycleConnectedProperty).toBool()) {
        return;
    }

    menu->setProperty(kMnemonicLifecycleConnectedProperty, true);
    connect(menu, &QMenu::aboutToShow, this, [this]() {
        set_mnemonics_visible(true);
    });
    connect(menu, &QMenu::aboutToHide, this, [this]() {
        hide_when_menu_navigation_finishes();
    });
}

void MenuMnemonicController::hide_when_menu_navigation_finishes()
{
    QTimer::singleShot(0, this, [this]() {
        if (!menu_navigation_active()) {
            set_mnemonics_visible(false);
        }
    });
}

bool MenuMnemonicController::menu_navigation_active() const
{
    if (menu_bar_ && menu_bar_->activeAction()) {
        return true;
    }

    for (QMenu* const menu : top_level_menus()) {
        if (menu && menu->isVisible()) {
            return true;
        }
    }
    return false;
}

QList<QMenu*> MenuMnemonicController::top_level_menus() const
{
    QList<QMenu*> menus;
    if (!menu_bar_) {
        return menus;
    }

    for (QAction* const action : menu_bar_->actions()) {
        if (!action || !action->menu()) {
            continue;
        }
        menus.push_back(action->menu());
    }
    return menus;
}
