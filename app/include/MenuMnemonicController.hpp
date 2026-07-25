#ifndef MENU_MNEMONIC_CONTROLLER_HPP
#define MENU_MNEMONIC_CONTROLLER_HPP

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

class QEvent;
class QMenu;
class QMenuBar;

/**
 * @brief Hides top-level menu mnemonic underlines until keyboard menu navigation is active.
 *
 * Qt renders ampersand mnemonic markers in menu titles on some Windows styles even before Alt
 * is pressed. This controller stores translated mnemonic titles, displays stripped titles by
 * default, and restores the mnemonic markers while Alt/menu navigation is active.
 */
class MenuMnemonicController final : public QObject {
public:
    /**
     * @brief Creates a controller for the given menu bar.
     * @param menu_bar Menu bar whose top-level menu titles are managed.
     * @param parent Optional QObject parent.
     */
    explicit MenuMnemonicController(QMenuBar* menu_bar, QObject* parent = nullptr);

    /**
     * @brief Destroys the controller and removes its application event filter.
     */
    ~MenuMnemonicController() override;

    /**
     * @brief Stores a translated title with mnemonic markers and applies the hidden state.
     * @param menu Menu whose title should be managed.
     * @param title Translated title, optionally containing ampersand mnemonic markers.
     */
    static void set_mnemonic_title(QMenu* menu, const QString& title);

    /**
     * @brief Returns the stored translated title with mnemonic markers.
     * @param menu Menu to inspect.
     * @return Stored mnemonic title, or the current menu title when none is stored.
     */
    static QString mnemonic_title(const QMenu* menu);

    /**
     * @brief Returns a copy of a label without Qt ampersand mnemonic markers.
     * @param value Label that may contain mnemonic markers.
     * @return Label suitable for normal pointer-driven menu display.
     */
    static QString strip_mnemonic_markers(const QString& value);

    /**
     * @brief Reapplies the current visibility state to all managed top-level menus.
     */
    void refresh_titles();

    /**
     * @brief Shows or hides mnemonic markers in managed top-level menu titles.
     * @param visible True to show access-key markers, false to hide them.
     */
    void set_mnemonics_visible(bool visible);

    /**
     * @brief Returns whether mnemonic markers are currently visible.
     * @return True when top-level menu titles include mnemonic markers.
     */
    bool mnemonics_visible() const { return mnemonics_visible_; }

protected:
    /**
     * @brief Observes Alt/menu events that enter or leave keyboard menu navigation.
     * @param watched Object receiving the event.
     * @param event Event being filtered.
     * @return Always false so normal Qt menu handling continues.
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /**
     * @brief Connects menu show/hide signals once so mnemonic visibility follows menu navigation.
     * @param menu Top-level menu whose lifecycle should be observed.
     */
    void connect_menu_lifecycle(QMenu* menu);
    /**
     * @brief Queues hiding mnemonic markers after Qt has finished processing menu state changes.
     */
    void hide_when_menu_navigation_finishes();
    /**
     * @brief Returns whether the menu bar or one of its top-level menus is still active.
     * @return True while keyboard/menu navigation is still in progress.
     */
    bool menu_navigation_active() const;
    /**
     * @brief Returns the current top-level menus owned by the managed menu bar.
     * @return List of top-level menus in menu-bar order.
     */
    QList<QMenu*> top_level_menus() const;

    QPointer<QMenuBar> menu_bar_;
    bool mnemonics_visible_{false};
};

#endif // MENU_MNEMONIC_CONTROLLER_HPP
