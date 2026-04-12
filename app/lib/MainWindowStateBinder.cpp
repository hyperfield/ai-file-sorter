#include "MainWindowStateBinder.hpp"

#include "CategorizationDialog.hpp"
#include "ErrorMessages.hpp"
#include "MainApp.hpp"
#include "VisualLlmRuntime.hpp"

#include <QAction>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolButton>

namespace {

void sync_disclosure_button(QToolButton* button, bool expanded)
{
    if (!button) {
        return;
    }
    Q_UNUSED(expanded);
    button->update();
}

} // namespace

MainWindowStateBinder::MainWindowStateBinder(MainApp& app)
    : app_(app)
{
}

void MainWindowStateBinder::connect_checkbox_signals()
{
    QObject::connect(app_.use_subcategories_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
        app_.settings.set_use_subcategories(checked);
        if (app_.categorization_dialog) {
            app_.categorization_dialog->set_show_subcategory_column(checked);
        }
    });

    if (app_.categorization_style_refined_radio) {
        QObject::connect(app_.categorization_style_refined_radio, &QRadioButton::toggled, &app_, [this](bool checked) {
            if (checked) {
                app_.set_categorization_style(false);
                app_.settings.set_use_consistency_hints(false);
            } else if (app_.categorization_style_consistent_radio &&
                       !app_.categorization_style_consistent_radio->isChecked()) {
                app_.set_categorization_style(true);
                app_.settings.set_use_consistency_hints(true);
            }
        });
    }

    if (app_.categorization_style_consistent_radio) {
        QObject::connect(app_.categorization_style_consistent_radio, &QRadioButton::toggled, &app_, [this](bool checked) {
            if (checked) {
                app_.set_categorization_style(true);
                app_.settings.set_use_consistency_hints(true);
            } else if (app_.categorization_style_refined_radio &&
                       !app_.categorization_style_refined_radio->isChecked()) {
                app_.set_categorization_style(false);
                app_.settings.set_use_consistency_hints(false);
            }
        });
    }

    QObject::connect(app_.categorize_files_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
        ensure_one_checkbox_active(app_.categorize_files_checkbox);
        update_file_scan_option(FileScanOptions::Files, checked);
        app_.settings.set_categorize_files(checked);
    });

    QObject::connect(app_.categorize_directories_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
        ensure_one_checkbox_active(app_.categorize_directories_checkbox);
        update_file_scan_option(FileScanOptions::Directories, checked);
        app_.settings.set_categorize_directories(checked);
    });

    if (app_.include_subdirectories_checkbox) {
        QObject::connect(app_.include_subdirectories_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            app_.settings.set_include_subdirectories(checked);
            if (checked &&
                app_.categorize_directories_checkbox &&
                app_.categorize_directories_checkbox->isChecked()) {
                QSignalBlocker blocker(app_.categorize_directories_checkbox);
                app_.categorize_directories_checkbox->setChecked(false);
                update_file_scan_option(FileScanOptions::Directories, false);
                app_.settings.set_categorize_directories(false);
            }
            update_image_only_controls();
        });
    }

    if (app_.analyze_images_checkbox) {
        QObject::connect(app_.analyze_images_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            handle_image_analysis_toggle(checked);
        });
    }

    if (app_.process_images_only_checkbox) {
        QObject::connect(app_.process_images_only_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            app_.settings.set_process_images_only(checked);
            update_image_only_controls();
        });
    }

    if (app_.add_image_date_to_category_checkbox) {
        QObject::connect(app_.add_image_date_to_category_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            app_.settings.set_add_image_date_to_category(checked);
        });
    }

    if (app_.add_image_date_place_to_filename_checkbox) {
        QObject::connect(app_.add_image_date_place_to_filename_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            app_.settings.set_add_image_date_place_to_filename(checked);
        });
    }

    if (app_.add_audio_video_metadata_to_filename_checkbox) {
        QObject::connect(app_.add_audio_video_metadata_to_filename_checkbox,
                         &QCheckBox::toggled,
                         &app_,
                         [this](bool checked) { app_.settings.set_add_audio_video_metadata_to_filename(checked); });
    }

    if (app_.offer_rename_images_checkbox) {
        QObject::connect(app_.offer_rename_images_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            if (!checked &&
                app_.rename_images_only_checkbox &&
                app_.rename_images_only_checkbox->isChecked()) {
                QSignalBlocker blocker(app_.rename_images_only_checkbox);
                app_.rename_images_only_checkbox->setChecked(false);
            }
            app_.settings.set_offer_rename_images(checked);
            if (app_.add_image_date_place_to_filename_checkbox) {
                app_.settings.set_add_image_date_place_to_filename(
                    app_.add_image_date_place_to_filename_checkbox->isChecked());
            }
            if (app_.rename_images_only_checkbox) {
                app_.settings.set_rename_images_only(app_.rename_images_only_checkbox->isChecked());
            }
            update_image_analysis_controls();
        });
    }

    if (app_.rename_images_only_checkbox) {
        QObject::connect(app_.rename_images_only_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            if (checked &&
                app_.offer_rename_images_checkbox &&
                !app_.offer_rename_images_checkbox->isChecked()) {
                QSignalBlocker blocker(app_.offer_rename_images_checkbox);
                app_.offer_rename_images_checkbox->setChecked(true);
            }
            app_.settings.set_rename_images_only(checked);
            if (app_.offer_rename_images_checkbox) {
                app_.settings.set_offer_rename_images(app_.offer_rename_images_checkbox->isChecked());
            }
            update_image_analysis_controls();
        });
    }

    if (app_.image_options_toggle_button) {
        QObject::connect(app_.image_options_toggle_button, &QToolButton::toggled, &app_, [this](bool) {
            app_.settings.set_image_options_expanded(app_.image_options_toggle_button->isChecked());
            update_image_analysis_controls();
        });
    }

    if (app_.analyze_documents_checkbox) {
        QObject::connect(app_.analyze_documents_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            app_.settings.set_analyze_documents_by_content(checked);
            update_document_analysis_controls();
        });
    }

    if (app_.process_documents_only_checkbox) {
        QObject::connect(app_.process_documents_only_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            app_.settings.set_process_documents_only(checked);
            update_document_analysis_controls();
        });
    }

    if (app_.offer_rename_documents_checkbox) {
        QObject::connect(app_.offer_rename_documents_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            if (!checked &&
                app_.rename_documents_only_checkbox &&
                app_.rename_documents_only_checkbox->isChecked()) {
                QSignalBlocker blocker(app_.rename_documents_only_checkbox);
                app_.rename_documents_only_checkbox->setChecked(false);
            }
            app_.settings.set_offer_rename_documents(checked);
            if (app_.rename_documents_only_checkbox) {
                app_.settings.set_rename_documents_only(app_.rename_documents_only_checkbox->isChecked());
            }
            update_document_analysis_controls();
        });
    }

    if (app_.rename_documents_only_checkbox) {
        QObject::connect(app_.rename_documents_only_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            if (checked &&
                app_.offer_rename_documents_checkbox &&
                !app_.offer_rename_documents_checkbox->isChecked()) {
                QSignalBlocker blocker(app_.offer_rename_documents_checkbox);
                app_.offer_rename_documents_checkbox->setChecked(true);
            }
            app_.settings.set_rename_documents_only(checked);
            if (app_.offer_rename_documents_checkbox) {
                app_.settings.set_offer_rename_documents(app_.offer_rename_documents_checkbox->isChecked());
            }
            update_document_analysis_controls();
        });
    }

    if (app_.add_document_date_to_category_checkbox) {
        QObject::connect(app_.add_document_date_to_category_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
            app_.settings.set_add_document_date_to_category(checked);
        });
    }

    if (app_.document_options_toggle_button) {
        QObject::connect(app_.document_options_toggle_button, &QToolButton::toggled, &app_, [this](bool) {
            app_.settings.set_document_options_expanded(app_.document_options_toggle_button->isChecked());
            update_document_analysis_controls();
        });
    }
}

void MainWindowStateBinder::connect_whitelist_signals()
{
    QObject::connect(app_.use_whitelist_checkbox, &QCheckBox::toggled, &app_, [this](bool checked) {
        if (app_.whitelist_selector) {
            app_.whitelist_selector->setEnabled(checked);
        }
        app_.settings.set_use_whitelist(checked);
        app_.apply_whitelist_to_selector();
    });

    QObject::connect(app_.whitelist_selector, &QComboBox::currentTextChanged, &app_, [this](const QString& name) {
        app_.settings.set_active_whitelist(name.toStdString());
        if (auto entry = app_.whitelist_store.get(name.toStdString())) {
            app_.settings.set_allowed_categories(entry->categories);
            app_.settings.set_allowed_subcategories(entry->subcategories);
        }
    });
}

void MainWindowStateBinder::sync_settings_to_ui()
{
    restore_tree_settings();
    restore_sort_folder_state();
    restore_file_scan_options();
    restore_file_explorer_visibility();
    restore_development_preferences();
}

void MainWindowStateBinder::restore_tree_settings()
{
    app_.use_subcategories_checkbox->setChecked(app_.settings.get_use_subcategories());
    app_.set_categorization_style(app_.settings.get_use_consistency_hints());
    if (app_.use_whitelist_checkbox) {
        app_.use_whitelist_checkbox->setChecked(app_.settings.get_use_whitelist());
    }
    if (app_.whitelist_selector) {
        app_.apply_whitelist_to_selector();
    }
    app_.categorize_files_checkbox->setChecked(app_.settings.get_categorize_files());
    app_.categorize_directories_checkbox->setChecked(app_.settings.get_categorize_directories());
    if (app_.include_subdirectories_checkbox) {
        app_.include_subdirectories_checkbox->setChecked(app_.settings.get_include_subdirectories());
    }
    if (app_.analyze_images_checkbox) {
        QSignalBlocker blocker(app_.analyze_images_checkbox);
        app_.analyze_images_checkbox->setChecked(app_.settings.get_analyze_images_by_content());
    }
    if (app_.process_images_only_checkbox) {
        QSignalBlocker blocker(app_.process_images_only_checkbox);
        app_.process_images_only_checkbox->setChecked(app_.settings.get_process_images_only());
    }
    if (app_.add_image_date_place_to_filename_checkbox) {
        QSignalBlocker blocker(app_.add_image_date_place_to_filename_checkbox);
        app_.add_image_date_place_to_filename_checkbox->setChecked(
            app_.settings.get_add_image_date_place_to_filename());
    }
    if (app_.add_audio_video_metadata_to_filename_checkbox) {
        QSignalBlocker blocker(app_.add_audio_video_metadata_to_filename_checkbox);
        app_.add_audio_video_metadata_to_filename_checkbox->setChecked(
            app_.settings.get_add_audio_video_metadata_to_filename());
    }
    if (app_.add_image_date_to_category_checkbox) {
        QSignalBlocker blocker(app_.add_image_date_to_category_checkbox);
        app_.add_image_date_to_category_checkbox->setChecked(
            app_.settings.get_add_image_date_to_category());
    }
    if (app_.offer_rename_images_checkbox) {
        QSignalBlocker blocker(app_.offer_rename_images_checkbox);
        app_.offer_rename_images_checkbox->setChecked(app_.settings.get_offer_rename_images());
    }
    if (app_.rename_images_only_checkbox) {
        QSignalBlocker blocker(app_.rename_images_only_checkbox);
        app_.rename_images_only_checkbox->setChecked(app_.settings.get_rename_images_only());
    }
    if (app_.image_options_toggle_button) {
        const bool expand_images = app_.settings.get_image_options_expanded();
        QSignalBlocker blocker(app_.image_options_toggle_button);
        app_.image_options_toggle_button->setChecked(expand_images);
    }
    if (app_.analyze_documents_checkbox) {
        QSignalBlocker blocker(app_.analyze_documents_checkbox);
        app_.analyze_documents_checkbox->setChecked(app_.settings.get_analyze_documents_by_content());
    }
    if (app_.process_documents_only_checkbox) {
        QSignalBlocker blocker(app_.process_documents_only_checkbox);
        app_.process_documents_only_checkbox->setChecked(app_.settings.get_process_documents_only());
    }
    if (app_.offer_rename_documents_checkbox) {
        QSignalBlocker blocker(app_.offer_rename_documents_checkbox);
        app_.offer_rename_documents_checkbox->setChecked(app_.settings.get_offer_rename_documents());
    }
    if (app_.rename_documents_only_checkbox) {
        QSignalBlocker blocker(app_.rename_documents_only_checkbox);
        app_.rename_documents_only_checkbox->setChecked(app_.settings.get_rename_documents_only());
    }
    if (app_.add_document_date_to_category_checkbox) {
        QSignalBlocker blocker(app_.add_document_date_to_category_checkbox);
        app_.add_document_date_to_category_checkbox->setChecked(
            app_.settings.get_add_document_date_to_category());
    }
    if (app_.document_options_toggle_button) {
        const bool expand_documents = app_.settings.get_document_options_expanded();
        QSignalBlocker blocker(app_.document_options_toggle_button);
        app_.document_options_toggle_button->setChecked(expand_documents);
    }
    update_image_analysis_controls();
    update_document_analysis_controls();
}

void MainWindowStateBinder::restore_sort_folder_state()
{
    const QString stored_folder = QString::fromStdString(app_.settings.get_sort_folder());
    QString effective_folder = stored_folder;

    if (effective_folder.isEmpty() || !QDir(effective_folder).exists()) {
        effective_folder = QDir::homePath();
    }

    app_.path_entry->setText(effective_folder);

    if (!effective_folder.isEmpty() && QDir(effective_folder).exists()) {
        app_.statusBar()->showMessage(app_.tr("Loaded folder %1").arg(effective_folder), 3000);
        app_.status_is_ready_ = false;
        app_.update_folder_contents(effective_folder);
        app_.focus_file_explorer_on_path(effective_folder);
    } else if (!stored_folder.isEmpty()) {
        app_.core_logger->warn("Sort folder path is invalid: {}", stored_folder.toStdString());
    }
}

void MainWindowStateBinder::restore_file_scan_options()
{
    app_.file_scan_options = FileScanOptions::None;
    if (app_.settings.get_categorize_files()) {
        app_.file_scan_options = app_.file_scan_options | FileScanOptions::Files;
    }
    if (app_.settings.get_categorize_directories()) {
        app_.file_scan_options = app_.file_scan_options | FileScanOptions::Directories;
    }
}

void MainWindowStateBinder::restore_file_explorer_visibility()
{
    const bool show_explorer = app_.settings.get_show_file_explorer();
    if (app_.file_explorer_dock) {
        app_.file_explorer_dock->setVisible(show_explorer);
    }
    if (app_.file_explorer_menu_action) {
        app_.file_explorer_menu_action->setChecked(show_explorer);
    }
    app_.update_results_view_mode();
}

void MainWindowStateBinder::restore_development_preferences()
{
    if (!app_.development_mode_ || !app_.development_prompt_logging_action) {
        return;
    }

    QSignalBlocker blocker(app_.development_prompt_logging_action);
    app_.development_prompt_logging_action->setChecked(app_.development_prompt_logging_enabled_);
}

void MainWindowStateBinder::sync_ui_to_settings()
{
    app_.settings.set_use_subcategories(app_.use_subcategories_checkbox->isChecked());
    if (app_.categorization_style_consistent_radio) {
        app_.settings.set_use_consistency_hints(app_.categorization_style_consistent_radio->isChecked());
    }
    if (app_.use_whitelist_checkbox) {
        app_.settings.set_use_whitelist(app_.use_whitelist_checkbox->isChecked());
    }
    if (app_.whitelist_selector) {
        app_.settings.set_active_whitelist(app_.whitelist_selector->currentText().toStdString());
    }
    app_.settings.set_categorize_files(app_.categorize_files_checkbox->isChecked());
    app_.settings.set_categorize_directories(app_.categorize_directories_checkbox->isChecked());
    if (app_.include_subdirectories_checkbox) {
        app_.settings.set_include_subdirectories(app_.include_subdirectories_checkbox->isChecked());
    }
    if (app_.analyze_images_checkbox) {
        app_.settings.set_analyze_images_by_content(app_.analyze_images_checkbox->isChecked());
    }
    if (app_.process_images_only_checkbox) {
        app_.settings.set_process_images_only(app_.process_images_only_checkbox->isChecked());
    }
    if (app_.add_image_date_place_to_filename_checkbox) {
        app_.settings.set_add_image_date_place_to_filename(
            app_.add_image_date_place_to_filename_checkbox->isChecked());
    }
    if (app_.add_audio_video_metadata_to_filename_checkbox) {
        app_.settings.set_add_audio_video_metadata_to_filename(
            app_.add_audio_video_metadata_to_filename_checkbox->isChecked());
    }
    if (app_.add_image_date_to_category_checkbox) {
        app_.settings.set_add_image_date_to_category(
            app_.add_image_date_to_category_checkbox->isChecked());
    }
    if (app_.offer_rename_images_checkbox) {
        app_.settings.set_offer_rename_images(app_.offer_rename_images_checkbox->isChecked());
    }
    if (app_.rename_images_only_checkbox) {
        app_.settings.set_rename_images_only(app_.rename_images_only_checkbox->isChecked());
    }
    if (app_.analyze_documents_checkbox) {
        app_.settings.set_analyze_documents_by_content(app_.analyze_documents_checkbox->isChecked());
    }
    if (app_.process_documents_only_checkbox) {
        app_.settings.set_process_documents_only(app_.process_documents_only_checkbox->isChecked());
    }
    if (app_.offer_rename_documents_checkbox) {
        app_.settings.set_offer_rename_documents(app_.offer_rename_documents_checkbox->isChecked());
    }
    if (app_.rename_documents_only_checkbox) {
        app_.settings.set_rename_documents_only(app_.rename_documents_only_checkbox->isChecked());
    }
    if (app_.add_document_date_to_category_checkbox) {
        app_.settings.set_add_document_date_to_category(
            app_.add_document_date_to_category_checkbox->isChecked());
    }
    const QByteArray folder_bytes = app_.path_entry->text().toUtf8();
    app_.settings.set_sort_folder(
        std::string(folder_bytes.constData(), static_cast<std::size_t>(folder_bytes.size())));
    if (app_.file_explorer_menu_action) {
        app_.settings.set_show_file_explorer(app_.file_explorer_menu_action->isChecked());
    }
    if (app_.consistency_pass_action) {
        app_.settings.set_consistency_pass_enabled(app_.consistency_pass_action->isChecked());
    }
    if (app_.development_mode_ && app_.development_prompt_logging_action) {
        const bool checked = app_.development_prompt_logging_action->isChecked();
        app_.development_prompt_logging_enabled_ = checked;
        app_.settings.set_development_prompt_logging(checked);
        app_.apply_development_logging();
    }
    if (app_.language_group) {
        if (QAction* checked = app_.language_group->checkedAction()) {
            app_.settings.set_language(static_cast<Language>(checked->data().toInt()));
        }
    }
}

void MainWindowStateBinder::ensure_one_checkbox_active(QCheckBox* changed_checkbox)
{
    if (!app_.categorize_files_checkbox || !app_.categorize_directories_checkbox) {
        return;
    }

    const bool include_subdirs_active = app_.include_subdirectories_checkbox &&
                                        app_.include_subdirectories_checkbox->isChecked();
    if (include_subdirs_active &&
        !app_.categorize_files_checkbox->isChecked() &&
        !app_.categorize_directories_checkbox->isChecked()) {
        return;
    }

    if (!app_.categorize_files_checkbox->isChecked() &&
        !app_.categorize_directories_checkbox->isChecked()) {
        QCheckBox* other = (changed_checkbox == app_.categorize_files_checkbox)
                               ? app_.categorize_directories_checkbox
                               : app_.categorize_files_checkbox;
        other->setChecked(true);
    }
}

void MainWindowStateBinder::update_file_scan_option(FileScanOptions option, bool enabled)
{
    if (enabled) {
        app_.file_scan_options = app_.file_scan_options | option;
    } else {
        app_.file_scan_options = app_.file_scan_options & ~option;
    }
}

FileScanOptions MainWindowStateBinder::effective_scan_options() const
{
    const bool analyze_images = app_.settings.get_analyze_images_by_content();
    const bool analyze_documents = app_.settings.get_analyze_documents_by_content();
    const bool images_only = analyze_images && app_.settings.get_process_images_only();
    const bool documents_only = analyze_documents && app_.settings.get_process_documents_only();
    if (images_only || documents_only) {
        return FileScanOptions::Files;
    }
    FileScanOptions options = app_.file_scan_options;
    if (analyze_images || analyze_documents) {
        options = options | FileScanOptions::Files;
    }
    if (app_.settings.get_include_subdirectories() && has_flag(options, FileScanOptions::Files)) {
        options = options | FileScanOptions::Recursive;
    }
    return options;
}

bool MainWindowStateBinder::visual_llm_files_available() const
{
#ifdef AI_FILE_SORTER_TEST_BUILD
    if (app_.visual_llm_available_probe_) {
        return app_.visual_llm_available_probe_();
    }
#endif
    return VisualLlmRuntime::resolve_active_backend(app_.settings.get_visual_model_id(),
                                                    nullptr)
        .has_value();
}

void MainWindowStateBinder::update_image_analysis_controls()
{
    if (!app_.analyze_images_checkbox ||
        !app_.process_images_only_checkbox ||
        !app_.add_image_date_to_category_checkbox ||
        !app_.add_image_date_place_to_filename_checkbox ||
        !app_.offer_rename_images_checkbox ||
        !app_.rename_images_only_checkbox) {
        return;
    }

    const bool analysis_enabled = app_.analyze_images_checkbox->isChecked();
    const bool rename_only = analysis_enabled && app_.rename_images_only_checkbox->isChecked();
    app_.process_images_only_checkbox->setEnabled(analysis_enabled);
    app_.offer_rename_images_checkbox->setEnabled(analysis_enabled);
    app_.rename_images_only_checkbox->setEnabled(analysis_enabled);
    app_.add_image_date_to_category_checkbox->setEnabled(analysis_enabled && !rename_only);
    app_.add_image_date_place_to_filename_checkbox->setEnabled(
        analysis_enabled && app_.offer_rename_images_checkbox->isChecked());
    if (app_.image_options_toggle_button) {
        app_.image_options_toggle_button->setEnabled(analysis_enabled);
        const bool expanded = app_.image_options_toggle_button->isChecked();
        sync_disclosure_button(app_.image_options_toggle_button, expanded);
        if (app_.image_options_container) {
            app_.image_options_container->setVisible(analysis_enabled && expanded);
        }
    } else if (app_.image_options_container) {
        app_.image_options_container->setVisible(analysis_enabled);
    }

    if (analysis_enabled &&
        app_.rename_images_only_checkbox->isChecked() &&
        !app_.offer_rename_images_checkbox->isChecked()) {
        QSignalBlocker blocker(app_.offer_rename_images_checkbox);
        app_.offer_rename_images_checkbox->setChecked(true);
    }

    update_image_only_controls();
}

void MainWindowStateBinder::update_image_only_controls()
{
    if (!app_.process_images_only_checkbox && !app_.process_documents_only_checkbox) {
        return;
    }

    const bool analyze_images = app_.analyze_images_checkbox && app_.analyze_images_checkbox->isChecked();
    const bool analyze_documents = app_.analyze_documents_checkbox && app_.analyze_documents_checkbox->isChecked();
    const bool images_only_active = app_.process_images_only_checkbox &&
                                    analyze_images &&
                                    app_.process_images_only_checkbox->isChecked();
    const bool documents_only_active = app_.process_documents_only_checkbox &&
                                       analyze_documents &&
                                       app_.process_documents_only_checkbox->isChecked();
    const bool rename_images_active = app_.rename_images_only_checkbox &&
                                      analyze_images &&
                                      app_.rename_images_only_checkbox->isChecked();
    const bool rename_documents_active = app_.rename_documents_only_checkbox &&
                                         analyze_documents &&
                                         app_.rename_documents_only_checkbox->isChecked();

    const bool restrict_types = images_only_active || documents_only_active;
    const bool allow_images = !restrict_types || images_only_active;
    const bool allow_documents = !restrict_types || documents_only_active;
    const bool allow_other_files = !restrict_types;
    const bool images_rename_only = allow_images ? rename_images_active : true;
    const bool documents_rename_only = allow_documents ? rename_documents_active : true;
    const bool disable_files_categorization =
        !allow_other_files && images_rename_only && documents_rename_only;
    const bool include_subdirs_active = app_.include_subdirectories_checkbox &&
                                        app_.include_subdirectories_checkbox->isChecked();
    const bool disable_directories_categorization = restrict_types || include_subdirs_active;

    if (app_.use_subcategories_checkbox) {
        app_.use_subcategories_checkbox->setEnabled(!disable_files_categorization);
    }
    if (app_.categorize_files_checkbox) {
        app_.categorize_files_checkbox->setEnabled(!disable_files_categorization);
    }
    if (app_.categorize_directories_checkbox) {
        app_.categorize_directories_checkbox->setEnabled(!disable_directories_categorization);
    }
    if (app_.categorization_style_heading) {
        app_.categorization_style_heading->setEnabled(!disable_files_categorization);
    }
    if (app_.categorization_style_refined_radio) {
        app_.categorization_style_refined_radio->setEnabled(!disable_files_categorization);
    }
    if (app_.categorization_style_consistent_radio) {
        app_.categorization_style_consistent_radio->setEnabled(!disable_files_categorization);
    }
    if (app_.use_whitelist_checkbox) {
        app_.use_whitelist_checkbox->setEnabled(!disable_files_categorization);
    }
    if (app_.whitelist_selector) {
        const bool whitelist_enabled = !disable_files_categorization &&
                                       app_.use_whitelist_checkbox &&
                                       app_.use_whitelist_checkbox->isChecked();
        app_.whitelist_selector->setEnabled(whitelist_enabled);
    }
}

void MainWindowStateBinder::update_document_analysis_controls()
{
    if (!app_.analyze_documents_checkbox ||
        !app_.process_documents_only_checkbox ||
        !app_.offer_rename_documents_checkbox ||
        !app_.rename_documents_only_checkbox ||
        !app_.add_document_date_to_category_checkbox) {
        return;
    }

    const bool analysis_enabled = app_.analyze_documents_checkbox->isChecked();
    app_.process_documents_only_checkbox->setEnabled(analysis_enabled);
    app_.offer_rename_documents_checkbox->setEnabled(analysis_enabled);
    app_.rename_documents_only_checkbox->setEnabled(analysis_enabled);

    const bool rename_only = analysis_enabled && app_.rename_documents_only_checkbox->isChecked();
    app_.add_document_date_to_category_checkbox->setEnabled(analysis_enabled && !rename_only);

    if (analysis_enabled &&
        app_.rename_documents_only_checkbox->isChecked() &&
        !app_.offer_rename_documents_checkbox->isChecked()) {
        QSignalBlocker blocker(app_.offer_rename_documents_checkbox);
        app_.offer_rename_documents_checkbox->setChecked(true);
    }

    if (app_.document_options_toggle_button) {
        app_.document_options_toggle_button->setEnabled(analysis_enabled);
        const bool expanded = app_.document_options_toggle_button->isChecked();
        sync_disclosure_button(app_.document_options_toggle_button, expanded);
        if (app_.document_options_container) {
            app_.document_options_container->setVisible(analysis_enabled && expanded);
        }
    } else if (app_.document_options_container) {
        app_.document_options_container->setVisible(analysis_enabled);
    }

    update_image_only_controls();
}

void MainWindowStateBinder::run_llm_selection_dialog_for_visual()
{
#ifdef AI_FILE_SORTER_TEST_BUILD
    if (app_.llm_selection_runner_override_) {
        app_.llm_selection_runner_override_();
        return;
    }
#endif
    app_.show_llm_selection_dialog();
}

void MainWindowStateBinder::handle_image_analysis_toggle(bool checked)
{
    if (!app_.analyze_images_checkbox) {
        return;
    }

    if (checked && !visual_llm_files_available()) {
        bool should_open_dialog = false;
#ifdef AI_FILE_SORTER_TEST_BUILD
        if (app_.image_analysis_prompt_override_) {
            should_open_dialog = app_.image_analysis_prompt_override_();
        } else
#endif
        {
            QMessageBox box(&app_);
            box.setIcon(QMessageBox::Information);
            box.setWindowTitle(app_.tr("Download required"));
            box.setText(app_.tr("Image analysis requires visual LLM files. Download them now?"));
            QPushButton* ok_button = box.addButton(app_.tr("OK"), QMessageBox::AcceptRole);
            box.addButton(QMessageBox::Cancel);
            box.setDefaultButton(ok_button);
            box.exec();
            should_open_dialog = (box.clickedButton() == ok_button);
        }

        if (!should_open_dialog) {
            QSignalBlocker blocker(app_.analyze_images_checkbox);
            app_.analyze_images_checkbox->setChecked(false);
            app_.settings.set_analyze_images_by_content(false);
            update_image_analysis_controls();
            return;
        }

        run_llm_selection_dialog_for_visual();

        if (!visual_llm_files_available()) {
            QSignalBlocker blocker(app_.analyze_images_checkbox);
            app_.analyze_images_checkbox->setChecked(false);
            app_.settings.set_analyze_images_by_content(false);
            update_image_analysis_controls();
            return;
        }
    }

    app_.settings.set_analyze_images_by_content(app_.analyze_images_checkbox->isChecked());
    update_image_analysis_controls();
}
