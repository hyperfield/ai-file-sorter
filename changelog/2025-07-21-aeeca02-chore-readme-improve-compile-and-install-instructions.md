# 2025-07-21: chore(readme): improve compile and install instructions

## Covered commits
- `aeeca02` `2025-07-21` `chore(readme): improve compile and install instructions`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `aeeca02`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -140,9 +140,23 @@ pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkm
 
 11. Run `make`, `make install` and `make clean`. The executable `AiFileSorter.exe` will be located in `C:\Program Files\AiFileSorter`. You can add the directory to `%PATH%`.
 
+To uninstall, launch `MSYS2 MINGW64` (**NOT** `MSYS2 MSYS`) *as Administrator*, go to the same directory (`ai-file-sorter/app`) and issue the command `make uninstall`.
+
 ---
 
-### MacOS (Apple Silicon)
+### MacOS
+
+##### Clone the repository
+
+    git clone https://github.com/hyperfield/ai-file-sorter.git
+    cd ai-file-sorter
+    git submodule update --init --recursive --remote
+
+##### Navigate into the directory
+
+    cd ai-file-sorter
+
+#### Compile the app
 
 1. Install Xcode (required for Accelerate.framework and AppleClang):
 - From the App Store, install **Xcode**.
@@ -154,38 +168,41 @@ pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkm
 
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
 
-3. Add these lines to your `~/.zshrc` file:
-
-```
-export PATH="/opt/homebrew/opt/curl/bin:$PATH"
-export PATH="/usr/bin:$PATH"
-export PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH
-export PKG_CONFIG_PATH=/opt/homebrew/share/pkgconfig:$PKG_CONFIG_PATH
-export PKG_CONFIG_PATH=/opt/homebrew/opt/libffi/lib/pkgconfig:$PKG_CONFIG_PATH
-export PKG_CONFIG_PATH=/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH
-export LDFLAGS="-L/opt/homebrew/opt/libffi/lib"
-export CPPFLAGS="-I/opt/homebrew/opt/libffi/include"
-```
-
-Then `source ~/.zshrc` or `source ~/.bashrc`.
-
-4. Install dependencies:
+3. Install dependencies:
    ```bash
-   brew install cmake gcc atkmm@2.28 cairo at-spi2-core pangomm@2.46 gtk+3 gtkmm3 glibmm@2.66 cairomm@1.14 pango harfbuzz glib gettext curl jsoncpp sqlite3 openssl@3 pkg-config libffi expat xproto xorgproto fmt spdlog adwaita-icon-theme hicolor-icon-theme
+   brew install cmake gcc atkmm@2.28 cairo at-spi2-core pangomm@2.46 gtk+3 gtkmm3 glibmm@2.66 cairomm@1.14 pango harfbuzz glib gettext curl jsoncpp sqlite3 openssl@3 pkg-config libffi expat xorgproto fmt spdlog adwaita-icon-theme hicolor-icon-theme
 
    brew install --cask font-0xproto
    ```
 
-5. Go to [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 6 here. The app won't work otherwise.
+4. Go to [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 6 here. The app won't work otherwise.
 
-6. Go to `app/resources` and run `./compile-resources.sh`. Go back to the `app` directory.
+5. Go to `app/resources` and run `./compile-resources.sh`. Go back to the `app` directory.
 
-7. Run `make`, `sudo make install`, `make clean`. Then you can launch the app with the command `aifilesorter`.
+6. Run `make`, `sudo make install`, `make clean`. Then you can launch the app with the command `aifilesorter`.
+
+---
+
+## Uninstallation
+
+Use `sudo make uninstall` in the same `app` subdirectory
 
 ---
 
 ### Linux
 
+##### Clone the repository
+
+    git clone https://github.com/hyperfield/ai-file-sorter.git
+    cd ai-file-sorter
+    git submodule update --init --recursive --remote
+
+##### Navigate into the directory
+
+    cd ai-file-sorter
+
+#### Compile the app
+
 #### 1. Install dependencies:
 
 ##### Debian / Ubuntu:
```

The excerpt is taken from the commit diff for `chore(readme): improve compile and install instructions`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
