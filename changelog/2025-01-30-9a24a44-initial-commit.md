# 2025-01-30: Initial commit

## Covered commits
- `9a24a44` `2025-01-30` `Initial commit`

## Motivation
This commit changed the project state in a way that was worth preserving in the backlog changelog even though the subject line does not map neatly to one category. The important part is the concrete repository delta it introduced.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `.gitignore`
- `A` `LICENSE`
- `A` `README.md`
- `A` `api-key-encryption/compile.sh`
- `A` `api-key-encryption/obfuscate_encrypt.cpp`
- `A` `app/Makefile`
- `A` `app/include/CategorizationDialog.hpp`
- `A` `app/include/CategorizationProgressDialog.hpp`
- `A` `app/include/CategorizationSession.hpp`
- `A` `app/include/CategorizedFile.hpp`
- `A` `app/include/CryptoManager.hpp`
- `A` `app/include/DatabaseManager.hpp`
- `A` `app/include/EmbeddedEnv.hpp`
- `A` `app/include/ErrorMessages.hpp`
- `A` `app/include/FileScanner.hpp`
- `A` `app/include/IniConfig.hpp`
- `A` `app/include/LLMClient.hpp`
- `A` `app/include/Logger.hpp`
- `A` `app/include/MainApp.hpp`
- `A` `app/include/MainAppEditActions.hpp`
- `A` `app/include/MainAppHelpActions.hpp`
- `A` `app/include/Settings.hpp`
- `A` `app/include/Updater.hpp`
- `A` `app/include/Utils.hpp`
- `A` `app/include/Version.hpp`
- `A` `app/include/app_version.hpp`
- `A` `app/include/constants.hpp`
- `A` `app/include/external/dotenv.h`
- `A` `app/includePaths.txt`
- `A` `app/lib/CategorizationDialog.cpp`
- `A` `app/lib/CategorizationProgressDialog.cpp`
- `A` `app/lib/CategorizationSession.cpp`
- `A` `app/lib/CategorizedFile.cpp`
- `A` `app/lib/CryptoManager.cpp`
- `A` `app/lib/DatabaseManager.cpp`
- `A` `app/lib/EmbeddedEnv.cpp`
- `A` `app/lib/FileScanner.cpp`
- `A` `app/lib/IniConfig.cpp`
- `A` `app/lib/LLMClient.cpp`
- `A` `app/lib/Logger.cpp`
- `A` `app/lib/MainApp.cpp`
- `A` `app/lib/MainAppEditActions.cpp`
- `A` `app/lib/MainAppHelpActions.cpp`
- `A` `app/lib/Settings.cpp`
- `A` `app/lib/Updater.cpp`
- `A` `app/lib/Utils.cpp`
- `A` `app/lib/Version.cpp`
- `A` `app/main.cpp`
- `A` `app/resources/.env`
- `A` `app/resources/compile-resources.sh`
- `A` `app/resources/exe_icon.ico`
- `A` `app/resources/exe_icon.rc`
- `A` `app/resources/images/app_icon_128.png`
- `A` `app/resources/images/app_icon_128.xcf`
- `A` `app/resources/images/app_icon_128_magnified.xcf`
- `A` `app/resources/images/app_icon_256.png`
- `A` `app/resources/images/app_icon_256.svg`
- `A` `app/resources/images/app_icon_256.xcf`
- `A` `app/resources/images/app_icon_256_arrows.png`
- `A` `app/resources/images/app_icon_256_magnified.xcf`
- `A` `app/resources/images/app_icon_350.xcf`
- `A` `app/resources/images/app_icon_512.xcf`
- `A` `app/resources/images/app_icon_64.png`
- `A` `app/resources/images/app_icon_arrows.svg`
- `A` `app/resources/images/app_icon_arrows_more_colours.xcf`
- `A` `app/resources/images/icon_128x128.png`
- `A` `app/resources/images/icon_16x16.png`
- `A` `app/resources/images/icon_256x256.png`
- `A` `app/resources/images/icon_32x32.png`
- `A` `app/resources/images/icon_512x512.png`
- `A` `app/resources/images/icon_64x64.png`
- `A` `app/resources/images/logo.png`
- `A` `app/resources/images/logo.xcf`
- `A` `app/resources/images/logo_bg.png`
- `A` `app/resources/images/logo_inscription.png`
- `A` `app/resources/images/logo_inscription.svg`
- `A` `app/resources/images/logo_inscription.xcf`
- `A` `app/resources/images/logo_qn.png`
- `A` `app/resources/images/logo_uncropped.png`
- `A` `app/resources/images/qn_logo.png`
- `A` `app/resources/images/qn_logo.svg`
- `A` `app/resources/images/qn_logo.xcf`
- `A` `app/resources/images/qn_logo_200.xcf`
- `A` `app/resources/resources.c`
- `A` `app/resources/resources.gresource`
- `A` `app/resources/resources.xml`
- `A` `app/resources/ui/categorization_progress.glade`
- `A` `app/resources/ui/main_window.glade`
- `A` `app/resources/ui/main_window_old_menu.glade`
- `A` `app/resources/ui/sort_confirm.glade`
- `A` `app/startapp.cpp`
- `A` `screenshots/AI-File-Sorter_screenshot-1.png`
- `A` `screenshots/AI-File-Sorter_screenshot-2.png`
- `A` `screenshots/AI-File-Sorter_screenshot-3.png`
- `A` `screenshots/AI-File-Sorter_screenshot-4.png`

## What changed from what, why, and how
The commit modified `.gitignore`, `LICENSE`, `README.md`, `api-key-encryption/compile.sh`, `api-key-encryption/obfuscate_encrypt.cpp`, `app/Makefile`, `app/include/CategorizationDialog.hpp`, `app/include/CategorizationProgressDialog.hpp`, and 87 more file(s). It changed the repository from the prior state to the state described by `Initial commit`.

Before this commit, the repository reflected the state immediately preceding `9a24a44`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.gitignore b/.gitignore
--- /dev/null
+++ b/.gitignore
@@ -0,0 +1,86 @@
+# Created by https://www.toptal.com/developers/gitignore/api/c++,visualstudiocode
+# Edit at https://www.toptal.com/developers/gitignore?templates=c++,visualstudiocode
+
+### C++ ###
+# Prerequisites
+*.d
+
+# Compiled Object files
+*.slo
+*.lo
+*.o
+*.obj
+
+# Precompiled Headers
+*.gch
+*.pch
+
+# Databases
+*.db
+
+# Environment variables
+# .env
+encryption.ini
+
+# Virtual environments
+.venv/
+
+# Compiled Dynamic libraries
+*.so
+*.dylib
+*.dll
+
+# Fortran module files
+*.mod
+*.smod
+
+# Compiled Static libraries
+*.lai
+*.la
+*.a
+*.lib
+
+# Executables
+*.exe
+*.out
+*.app
+main
+obfuscate_encrypt
+
+# Scripts
+reset_key_4_github.sh
+
+# References
+# includePaths.txt
+
+### VisualStudioCode ###
+.vscode/
+.vscode/*
+!.vscode/settings.json
+!.vscode/tasks.json
+!.vscode/launch.json
+!.vscode/extensions.json
+!.vscode/*.code-snippets
+
+# Local History for Visual Studio Code
+.history/
+
+# Built Visual Studio Code Extensions
+*.vsix
+
+### VisualStudioCode Patch ###
+# Ignore all local history of files
+.history
+.ionide
+
+# Temp files
+*.*~
+*~
+
+# Temp notes
+todos_md/
+
+# Mac files
+.DS_Store
+
+# End of https://www.toptal.com/developers/gitignore/api/c++,visualstudiocode
diff --git a/LICENSE b/LICENSE
new file mode 100644
index 0000000..2beb9e1
--- /dev/null
+++ b/LICENSE
@@ -0,0 +1,662 @@
+                    GNU AFFERO GENERAL PUBLIC LICENSE
+                       Version 3, 19 November 2007
+
+ Copyright (C) 2007 Free Software Foundation, Inc. <https://fsf.org/>
```

The excerpt is taken from the commit diff for `Initial commit`. The most relevant surfaces are `.gitignore`, `LICENSE`, `README.md`, `api-key-encryption/compile.sh`, `api-key-encryption/obfuscate_encrypt.cpp`, and 90 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
