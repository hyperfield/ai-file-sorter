# 2026-01-16: docs(ui): add Doxygen comments for CustomLLMDialog

## Covered commits
- `6dc6974` `2026-01-16` `docs(ui): add Doxygen comments for CustomLLMDialog`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/CustomLLMDialog.hpp`

## What changed from what, why, and how
The commit updated documentation artifacts touching `app/include/CustomLLMDialog.hpp`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `6dc6974`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/CustomLLMDialog.hpp b/app/include/CustomLLMDialog.hpp
--- a/app/include/CustomLLMDialog.hpp
+++ b/app/include/CustomLLMDialog.hpp
@@ -9,19 +9,50 @@ class QLineEdit;
 class QPushButton;
 class QTextEdit;
 
+/**
+ * @brief Dialog for creating or editing custom local LLM entries.
+ */
 class CustomLLMDialog : public QDialog
 {
 public:
+    /**
+     * @brief Construct a dialog for a new custom LLM entry.
+     * @param parent Parent widget.
+     */
     explicit CustomLLMDialog(QWidget* parent = nullptr);
+    /**
+     * @brief Construct a dialog pre-populated with an existing entry.
+     * @param parent Parent widget.
+     * @param existing Existing custom LLM values to edit.
+     */
     explicit CustomLLMDialog(QWidget* parent, const CustomLLM& existing);
 
+    /**
+     * @brief Return the dialog values as a CustomLLM entry.
+     */
     CustomLLM result() const;
 
 private:
+    /**
+     * @brief Build the dialog layout and widgets.
+     */
     void setup_ui();
+    /**
+     * @brief Connect widget signals to validation and handlers.
+     */
     void wire_signals();
+    /**
+     * @brief Apply existing values to the input fields.
+     * @param existing Existing custom LLM values to load.
+     */
     void apply_existing(const CustomLLM& existing);
+    /**
+     * @brief Validate inputs and update the ok button state.
+     */
     void validate_inputs();
+    /**
+     * @brief Open a file picker to select a local model file.
+     */
     void browse_for_file();
 
     QLineEdit* name_edit{nullptr};
```

The excerpt is taken from the commit diff for `docs(ui): add Doxygen comments for CustomLLMDialog`. The most relevant surfaces are `app/include/CustomLLMDialog.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
