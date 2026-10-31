# Quick Start Guide

AI File Sorter helps you organize files with a review-first workflow and optional auto-approval to preselect changes in the review list.

The AI suggests categories, destination folders, and names. The app performs moves or renames only after you confirm the selected changes in the review dialog.

## Safe First Run

If this is your first time using AI File Sorter, start with a small test folder before pointing it at a large archive or drive.

Good first-run folders include:

- a copy of 20-50 files from `Downloads`
- a small screenshot or photo cleanup folder
- a temporary folder with a few PDFs or documents

This keeps the first run easy to review. Your files stay on your computer when you use local models, and the AI only suggests categories and names. Leave auto-approval options off for your first run so nothing is moved or renamed until you approve the review list.

## Set Up an LLM

Open **Settings -> Select LLM**. Choose a model from the **Local LLMs** drop-down or a provider from **API connections**.

**Qwen3.5 4B (Recommended)** is the first local option and the default for new setups. It offers high precision for categorization and is recommended for existing folder structures. Your previously saved model selection is preserved. To use your own GGUF file, choose **Custom local LLM (gguf)** and use **Add**.

For a built-in local model, expand **Downloads**, click **Download**, and wait for **Model ready** before confirming with **OK**. **Model storage** lets you choose the download folder. **See details** reveals the download URL, local path, and technical information.

For image analysis, also choose a **Visual model** under **Image analysis models** and download all of its required files. This selection is separate from the text model.

Under **API connections**, choose Gemini, ChatGPT (OpenAI), or a custom OpenAI-compatible endpoint, then enter the required connection details and model name. Cloud APIs require internet access and send analysis inputs to the configured service. Custom endpoints can also run locally.

## Check Local Performance (Optional)

After downloading models, open **File -> System compatibility check** and click **Run benchmark**. Close other CPU- or GPU-intensive apps first. The check tests downloaded local models, including custom GGUF models.

Leave **Backend** at **Automatic (try CPU if GPU fails)**, or choose **All available backends** to compare CPU and supported GPU options. Results are saved separately for each model and backend; a failure on one does not exclude a working combination on another.

Select a result and use **Retry selected** to repeat that combination, **Retry failed** to repeat failed combinations, or **Try chosen backend** to test the selected model with a specific backend from the drop-down.

Recommendations estimate local performance. The check does not test API services or accuracy when choosing existing folders, and it does not change your selected model automatically.

## 1. Choose a Folder

Use **Browse** or the **File Explorer** pane to pick the folder you want to sort.

Typical examples:

- `Downloads`
- a desktop cleanup folder
- an external drive folder
- a network or cloud-synced folder
- a project archive

**Destination** controls where sorted files go. Leave **Use analyzed folder** enabled to sort within the selected folder, or turn it off and browse to another destination.

## 2. Pick What the App Should Do

Use the main options to decide whether the app should:

- categorize files into folders
- analyze images
- analyze documents
- offer rename suggestions for supported files

If you only want rename suggestions, enable the relevant rename-only mode.

## 3. Choose How to Sort

Choose a sorting mode:

- **Create category folders** creates folders from suggested category names.
- **Use existing folder structure** chooses folders within your **Destination** tree. Enable **Suggest new folders when needed** to allow new destinations when none fits well; leave it off to use existing folders only.

For existing folders, we recommend **Qwen3.5 4B**. **Gemma 1.1 7B** and models accessed through an API are also suitable options. Other local models show an advisory; you can keep your model or change it in **Settings -> Select LLM**.

In **Create category folders** mode, choose a categorization style:

- **More refined** for general use and finer grouping
- **More consistent** if you want stronger label consistency across similar files

You can also enable category whitelists if you want the app to stay within a narrower set of category names.

## 4. Start the Analysis

Click **Analyze folder**.

The app scans the selected folder, gathers the information it needs, and prepares a review list.

## 5. Review Before Applying Changes

The review dialog lets you inspect:

- suggested categories
- optional subcategories
- rename suggestions for supported files
- the final destination paths
- file previews where supported

You can adjust or reject suggestions before confirming anything.

Enable **Dry run (preview only, do not move files)** to preview the planned moves and renames without applying them.

## 6. Apply the Changes

Select the rows you want to process and click **Confirm and Process**. The app creates any required folders and performs the selected moves or renames.

**Auto-approve filename changes** and **Auto-approve categorization** preselect eligible rows. You still need to confirm before those changes are applied.

## 7. Undo the Last Run

If you apply changes and then want to reverse them, use **Edit -> Undo last run**.

Undo is designed for the most recent confirmed sorting run. It uses the app's recorded run history to move files back and reverse supported renames where possible.

For best results, use Undo before starting another large cleanup in the same folder.

## 8. Learning from Your Reviews

When you approve categories in the review dialog, the app can remember those local decisions and use them as hints for future runs. This does not train or modify the AI model.

The learned examples are stored in a separate local database, so clearing the normal categorization cache does not remove them. To remove this local learning data, use **Settings -> Reset learned behavior**.

## Good to Know

- The app uses a local cache to avoid reprocessing the same files and to improve consistency.
- Auto-approval is optional. Leave it off until you are comfortable with the app's suggestions.
- Image and document options can be expanded separately if you need more control.

## If Something Looks Wrong

Check the following first:

- the selected folder is the one you intended
- the relevant analysis options are enabled
- rename-only mode is not limiting the result in a way you did not expect
- a category whitelist is not narrowing the suggestions too much

For additional troubleshooting, open **Help -> FAQ**.
