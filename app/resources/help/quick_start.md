# Quick Start Guide

AI File Sorter helps you organize files after your review and approval.

The AI drives the analysis and suggests categories, subcategories, and names. It does not directly touch your files. The app performs any moves or renames only after you confirm the reviewed changes.

## 1. Choose a Folder

Use **Browse** to pick the folder you want to sort.

Typical examples:

- `Downloads`
- a desktop cleanup folder
- an external drive folder
- a project archive

## 2. Pick What the App Should Do

Use the main options to decide whether the app should:

- categorize files into folders
- analyze images
- analyze documents
- offer rename suggestions for supported files

If you only want rename suggestions, enable the relevant rename-only mode.

## 3. Select Your Categorization Style

Choose the style that best matches your goal:

- **More refined** for general use and finer grouping
- **More consistent** if you want stronger label consistency across similar files

You can also enable category whitelists if you want the app to stay within a narrower set of category names.

## 4. Start the Analysis

Click **Analyze and categorize files**.

The app scans the selected folder, gathers the information it needs, and prepares a review list.

## 5. Review Before Applying Changes

The review dialog lets you inspect:

- suggested categories
- optional subcategories
- rename suggestions for supported files
- the final destination paths

You can adjust or reject suggestions before confirming anything.

## 6. Apply the Changes

Once you confirm, the app creates the required folders and performs the moves or renames.

## 7. Undo the Last Run

If you apply changes and then want to reverse them, use **Undo last run** from the menu.

Undo is designed for the most recent confirmed sorting run. It uses the app's recorded run history to move files back and reverse supported renames where possible.

For best results, use Undo before starting another large cleanup in the same folder.

## 8. Learning from Your Reviews

When you approve categories in the review dialog, the app can remember those local decisions and use them as hints for future runs. This does not train or modify the AI model.

The learned examples are stored in a separate local database, so clearing the normal categorization cache does not remove them. To remove this local learning data, use **Settings -> Reset learned behavior**.

## Good to Know

- The app uses a local cache to avoid reprocessing the same files and to improve consistency.
- The app does not automatically apply changes without showing you the review step first.
- Image and document options can be expanded separately if you need more control.

## If Something Looks Wrong

Check the following first:

- the selected folder is the one you intended
- the relevant analysis options are enabled
- rename-only mode is not limiting the result in a way you did not expect
- a category whitelist is not narrowing the suggestions too much

For additional troubleshooting, open **Help -> FAQ**.
