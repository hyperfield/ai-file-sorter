#pragma once

#include <QDialog>
#include <QCoreApplication>

class QEvent;
class QGroupBox;
class QLabel;
class QSlider;
class QShowEvent;
class Settings;

/** @brief Hosts sorting controls and independent content-derived filename limits. */
class SortingRulesDialog final : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(SortingRulesDialog)

public:
    /**
     * @brief Creates a persistent dialog backed by the application's settings.
     * @param settings Settings updated by the word-limit sliders.
     * @param parent Owning main window.
     */
    explicit SortingRulesDialog(Settings& settings, QWidget* parent = nullptr);

    /**
     * @brief Returns the panel for the existing sorting controls.
     * @return Dialog-owned panel populated by the UI builder.
     */
    QWidget* rules_widget() const;

protected:
    /**
     * @brief Refreshes the dialog's labels when the application language changes.
     * @param event Widget change event.
     */
    void changeEvent(QEvent* event) override;
    /**
     * @brief Synchronizes sliders after settings have been loaded or changed externally.
     * @param event Dialog show event.
     */
    void showEvent(QShowEvent* event) override;

private:
    /** @brief Updates translated labels and accessible slider names. */
    void retranslate();

    Settings& settings_;
    QWidget* rules_widget_{nullptr};
    QGroupBox* filename_group_{nullptr};
    QLabel* image_label_{nullptr};
    QLabel* document_label_{nullptr};
    QLabel* image_value_{nullptr};
    QLabel* document_value_{nullptr};
    QSlider* image_slider_{nullptr};
    QSlider* document_slider_{nullptr};
};
