#ifndef CATEGORIZATIONDIALOG_HPP
#define CATEGORIZATIONDIALOG_HPP

#include "CategoryLanguage.hpp"
#include "Types.hpp"

#include <QCoreApplication>
#include <QDialog>
#include <QStandardItemModel>

#include <memory>
#include <optional>
#include <tuple>
#include <vector>
#include <spdlog/logger.h>

class DatabaseManager;
class IFilePreviewService;
class IStorageProvider;
class ReviewHistoryStore;
class UserLearningStore;
class QCloseEvent;
class QEvent;
class QModelIndex;
class QPushButton;
class QTableView;
class QCheckBox;
class QStandardItem;

class CategorizationDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(CategorizationDialog)
public:
    /**
     * @brief Create the review dialog using the local filesystem provider.
     * @param db_manager Categorization cache/taxonomy database manager.
     * @param show_subcategory_col Whether the subcategory column is visible initially.
     * @param undo_dir Directory used to persist undo move plans.
     * @param category_language Language used for displayed category labels.
     * @param parent Parent widget.
     * @param learning_store Optional store for user-approved learning examples.
     * @param history_store Optional store for applied rename/categorization history.
     */
    CategorizationDialog(DatabaseManager* db_manager,
                         bool show_subcategory_col,
                         const std::string& undo_dir,
                         CategoryLanguage category_language = CategoryLanguage::English,
                         QWidget* parent = nullptr,
                         UserLearningStore* learning_store = nullptr,
                         ReviewHistoryStore* history_store = nullptr);
    /**
     * @brief Create the review dialog using an explicit storage provider.
     * @param db_manager Categorization cache/taxonomy database manager.
     * @param storage_provider Provider responsible for move/undo operations.
     * @param show_subcategory_col Whether the subcategory column is visible initially.
     * @param undo_dir Directory used to persist undo move plans.
     * @param category_language Language used for displayed category labels.
     * @param parent Parent widget.
     * @param learning_store Optional store for user-approved learning examples.
     * @param history_store Optional store for applied rename/categorization history.
     */
    CategorizationDialog(DatabaseManager* db_manager,
                         IStorageProvider& storage_provider,
                         bool show_subcategory_col,
                         const std::string& undo_dir,
                         CategoryLanguage category_language = CategoryLanguage::English,
                         QWidget* parent = nullptr,
                         UserLearningStore* learning_store = nullptr,
                         ReviewHistoryStore* history_store = nullptr);
    ~CategorizationDialog() override;

    void set_show_subcategory_column(bool enabled);
    bool show_subcategory_column_enabled() const { return show_subcategory_column; }
    /**
     * @brief Enables or disables automatic approval of filename changes.
     * @param enabled True to preselect actionable rows that rename files.
     */
    void set_review_auto_approve_filename_changes_enabled(bool enabled);
    /**
     * @brief Returns whether filename changes are automatically approved.
     * @return True when actionable filename changes are preselected for processing.
     */
    bool review_auto_approve_filename_changes_enabled() const;
    /**
     * @brief Enables or disables automatic approval of categorization moves.
     * @param enabled True to preselect actionable rows that categorize files.
     */
    void set_review_auto_approve_categorization_enabled(bool enabled);
    /**
     * @brief Returns whether categorization moves are automatically approved.
     * @return True when actionable categorization rows are preselected for processing.
     */
    bool review_auto_approve_categorization_enabled() const;
    /**
     * @brief Replace the file preview service used by the review dialog.
     * @param preview_service Service to use. Passing nullptr restores the default implementation.
     */
    void set_file_preview_service(std::unique_ptr<IFilePreviewService> preview_service);

#ifdef AI_FILE_SORTER_TEST_BUILD
    void test_set_entries(const std::vector<CategorizedFile>& files);
    void test_trigger_confirm();
    bool test_trigger_preview(int row);
    void test_trigger_undo();
    bool test_undo_enabled() const;
#endif

    bool is_dialog_valid() const;
    /**
     * @brief Populate and display categorized results in the review dialog.
     * @param categorized_files Results to review.
     * @param base_dir_override Optional base directory override for generated destinations.
     * @param include_subdirectories Whether results came from a recursive scan.
     * @param allow_image_renames Whether image rename-only controls are allowed.
     * @param allow_document_renames Whether document rename-only controls are allowed.
     * @param auto_approve_filename_changes Whether actionable filename changes should be preselected.
     * @param auto_approve_categorization Whether actionable categorization rows should be preselected.
     */
    void show_results(const std::vector<CategorizedFile>& categorized_files,
                      const std::string& base_dir_override = std::string(),
                      bool include_subdirectories = false,
                      bool allow_image_renames = true,
                      bool allow_document_renames = true,
                      bool auto_approve_filename_changes = false,
                      bool auto_approve_categorization = false);

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    enum class RowStatus {
        None = 0,
        Moved,
        Renamed,
        RenamedAndMoved,
        Skipped,
        NotSelected,
        Preview
    };

    static constexpr int kStatusRole = Qt::UserRole + 100;
    static constexpr int kFilePathRole = Qt::UserRole + 1;
    static constexpr int kUsedConsistencyRole = Qt::UserRole + 2;
    static constexpr int kRenameOnlyRole = Qt::UserRole + 3;
    static constexpr int kFileTypeRole = Qt::UserRole + 4;
    static constexpr int kRenameAppliedRole = Qt::UserRole + 5;
    static constexpr int kRenameLockedRole = Qt::UserRole + 6;
    static constexpr int kHiddenCategoryRole = Qt::UserRole + 7;
    static constexpr int kHiddenSubcategoryRole = Qt::UserRole + 8;
    static constexpr int kOriginalFileNameRole = Qt::UserRole + 9;
    static constexpr int kOriginalCategoryRole = Qt::UserRole + 10;
    static constexpr int kOriginalSubcategoryRole = Qt::UserRole + 11;
    static constexpr int kCanonicalCategoryRole = Qt::UserRole + 12;
    static constexpr int kCanonicalSubcategoryRole = Qt::UserRole + 13;
    static constexpr int kLearningContextRole = Qt::UserRole + 14;

    enum Column {
        ColumnSelect = 0,
        ColumnFile = 1,
        ColumnType = 2,
        ColumnSuggestedName = 3,
        ColumnCategory = 4,
        ColumnSubcategory = 5,
        ColumnStatus = 6,
        ColumnPreview = 7
    };

    struct MoveRecord {
        int row_index;
        std::string source_path;
        std::string destination_path;
        std::uintmax_t size_bytes{0};
        std::time_t mtime{0};
        std::string stable_identity;
        std::string revision_token;
        long long history_id{0};
    };
    struct PreviewRecord {
        std::string source;
        std::string destination;
        std::string source_file_name;
        std::string destination_file_name;
        std::string category;
        std::string subcategory;
        bool use_subcategory{false};
        bool rename_only{false};
    };

    /**
     * @brief Reapplies palette-aware styles after theme changes.
     */
    void apply_theme_styles();
    void setup_ui();
    void populate_model();
    void ensure_unique_suggested_names_in_model();
    /**
     * @brief Persist reviewed categorization results and optionally record approved learning examples.
     * @param learn_approved_mappings True to store user-approved mappings in the learning store.
     */
    void record_categorization_to_db(bool learn_approved_mappings = false);
    void on_confirm_and_sort_button_clicked();
    void on_continue_later_button_clicked();
    void on_undo_button_clicked();
    void show_close_button();
    void restore_action_buttons();
    void update_status_column(int row,
                              bool success,
                              bool attempted = true,
                              bool renamed = false,
                              bool moved = false);
    void on_select_all_toggled(bool checked);
    /**
     * @brief Applies the filename auto-approval policy when toggled.
     * @param checked True when filename changes should be preselected.
     */
    void on_auto_approve_filename_changes_toggled(bool checked);
    /**
     * @brief Applies the categorization auto-approval policy when toggled.
     * @param checked True when categorization rows should be preselected.
     */
    void on_auto_approve_categorization_toggled(bool checked);
    void on_preview_button_clicked();
    void on_table_double_clicked(const QModelIndex& index);
    /**
     * @brief Selects all highlighted rows for processing.
     */
    void on_select_highlighted_clicked();
    void apply_select_all(bool checked);
    /**
     * @brief Applies a check state to the given rows in the Process column.
     */
    void apply_check_state_to_rows(const std::vector<int>& rows, Qt::CheckState state);
    /**
     * @brief Applies automatic approval to every row in the model.
     */
    void apply_auto_approval_to_rows();
    /**
     * @brief Updates automatic approval for one row after the row changes.
     * @param row Row index in the model.
     */
    void update_auto_approval_for_row(int row);
    /**
     * @brief Returns whether a row is safe to preselect for processing.
     * @param row Row index in the model.
     * @return True when every action in the row is covered by the enabled auto-approval options.
     */
    bool row_is_auto_approval_candidate(int row) const;
    void on_item_changed(QStandardItem* item);
    void update_select_all_state();
    void update_type_icon(QStandardItem* item);
    void retranslate_ui();
    void apply_status_text(QStandardItem* item) const;
    RowStatus status_from_item(const QStandardItem* item) const;
    void on_show_subcategories_toggled(bool checked);
    void apply_subcategory_visibility();
    void clear_move_history();
    void record_move_for_undo(int row,
                              const std::string& source,
                              const std::string& destination,
                              std::uintmax_t size_bytes,
                              std::time_t mtime,
                              const std::string& stable_identity,
                              const std::string& revision_token,
                              long long history_id = 0);
    void handle_selected_row(int row_index,
                             const std::string& file_name,
                             const std::string& rename_candidate,
                             const std::string& category,
                             const std::string& subcategory,
                             const std::string& source_dir,
                             const std::string& base_dir,
                             std::vector<std::string>& files_not_moved,
                             FileType file_type,
                             bool rename_only,
                             bool used_consistency_hints,
                             bool dry_run);
    void persist_move_plan();
    bool undo_move_history();
    void update_status_after_undo();
    bool move_file_back(const std::string& source, const std::string& destination);
    /**
     * @brief Records a successful row mutation in the persistent review history store.
     * @param row Row index in the model.
     * @param operation Operation type to store.
     * @param source Original path before the move.
     * @param destination Final path after the move.
     * @param original_file_name Original file name.
     * @param final_file_name Final file name.
     * @param category Category label for categorization operations.
     * @param subcategory Subcategory label for categorization operations.
     * @param size_bytes File size captured after moving.
     * @param mtime Modification timestamp captured after moving.
     * @param stable_identity Provider identity captured after moving.
     * @param revision_token Provider revision token captured after moving.
     * @return Persisted history row id, or 0 when history was not recorded.
     */
    long long record_review_history(int row,
                                    const std::string& operation,
                                    const std::string& source,
                                    const std::string& destination,
                                    const std::string& original_file_name,
                                    const std::string& final_file_name,
                                    const std::string& category,
                                    const std::string& subcategory,
                                    std::uintmax_t size_bytes,
                                    std::time_t mtime,
                                    const std::string& stable_identity,
                                    const std::string& revision_token);
    /**
     * @brief Returns user-facing image/document description text for a row.
     * @param row Row index in the model.
     * @return Description or summary text captured during analysis.
     */
    std::string history_description_for_row(int row) const;
    void remove_empty_parent_directories(const std::string& destination);
    void set_preview_status(int row, const std::string& destination);
    void update_preview_column(int row);
    std::optional<std::string> compute_preview_path(int row) const;
    std::optional<PreviewRecord> build_preview_record_for_row(int row, std::string* debug_reason = nullptr) const;
    std::string resolve_destination_name(const std::string& original_name,
                                         const std::string& rename_candidate) const;
    bool validate_filename(const std::string& name, std::string& error) const;
    bool resolve_row_flags(int row, bool& rename_only, bool& used_consistency_hints, FileType& file_type) const;
    void set_show_rename_column(bool enabled);
    void apply_rename_visibility();
    void apply_category_visibility();
    /**
     * @brief Hides rows that are rename-only when required by the dialog mode.
     */
    void apply_rename_only_row_visibility();
    /**
     * @brief Syncs the rename-only checkbox state to current UI options.
     */
    void update_rename_only_checkbox_state();
    /**
     * @brief Enables/disables the subcategory checkbox based on rename-only mode.
     */
    void update_subcategory_checkbox_state();
    /**
     * @brief Marks image rows as rename-only when toggled.
     * @param checked True when image rename-only is enabled.
     */
    void on_rename_images_only_toggled(bool checked);
    /**
     * @brief Marks document rows as rename-only when toggled.
     * @param checked True when document rename-only is enabled.
     */
    void on_rename_documents_only_toggled(bool checked);
    bool row_is_already_renamed_with_category(int row) const;
    /**
     * @brief Returns true if the row points to a supported image file.
     * @param row Row index in the model.
     * @return True when the row is an image file supported by the visual analyzer.
     */
    bool row_is_supported_image(int row) const;
    /**
     * @brief Returns true if the row points to a supported document file.
     * @param row Row index in the model.
     * @return True when the row is a supported document file.
     */
    bool row_is_supported_document(int row) const;
    /**
     * @brief Returns unique row indices that are highlighted in the table view.
     */
    std::vector<int> selected_row_indices() const;
    /**
     * @brief Returns the row that should be previewed from the current selection context.
     * @return Row index to preview, or std::nullopt when no row is available.
     */
    std::optional<int> current_preview_row() const;
    /**
     * @brief Open a preview for the given model row.
     * @param row Row index in the model.
     * @return True when preview launch succeeded.
     */
    bool preview_row(int row);
    /**
     * @brief Updates preview action availability based on the current model content.
     */
    void update_preview_button_state();
    /**
     * @brief Opens a dialog to bulk edit categories for highlighted rows.
     */
    void on_bulk_edit_clicked();

    DatabaseManager* db_manager;
    UserLearningStore* learning_store_{nullptr};
    ReviewHistoryStore* history_store_{nullptr};
    IStorageProvider* storage_provider_{nullptr};
    CategoryLanguage category_language_{CategoryLanguage::English};
    bool show_subcategory_column;
    bool include_subdirectories_{false};
    bool allow_image_renames_{true};
    bool allow_document_renames_{true};
    bool show_rename_column{false};
    std::vector<CategorizedFile> categorized_files;

    std::shared_ptr<spdlog::logger> core_logger;
    std::shared_ptr<spdlog::logger> db_logger;
    std::shared_ptr<spdlog::logger> ui_logger;

    QTableView* table_view{nullptr};
    QStandardItemModel* model{nullptr};
    QPushButton* confirm_button{nullptr};
    QPushButton* continue_button{nullptr};
    QPushButton* close_button{nullptr};
    QPushButton* preview_button{nullptr};
    QCheckBox* select_all_checkbox{nullptr};
    QCheckBox* auto_approve_filenames_checkbox{nullptr};
    QCheckBox* auto_approve_categories_checkbox{nullptr};
    QPushButton* select_highlighted_button{nullptr};
    QPushButton* bulk_edit_button{nullptr};
    QCheckBox* show_subcategories_checkbox{nullptr};
    QCheckBox* dry_run_checkbox{nullptr};
    QCheckBox* rename_images_only_checkbox{nullptr};
    QCheckBox* rename_documents_only_checkbox{nullptr};
    QPushButton* undo_button{nullptr};

    std::vector<MoveRecord> move_history_;
    std::vector<PreviewRecord> dry_run_plan_;

    bool updating_select_all{false};
    bool suppress_item_changed_{false};
    bool auto_approve_filename_changes_{false};
    bool auto_approve_categorization_{false};
    std::string undo_dir_;
    std::string base_dir_;
    std::unique_ptr<IFilePreviewService> preview_service_;
};

#endif // CATEGORIZATIONDIALOG_HPP
