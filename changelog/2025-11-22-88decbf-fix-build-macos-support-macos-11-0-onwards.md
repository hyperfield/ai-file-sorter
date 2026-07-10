# 2025-11-22: fix(build-macos): support macOS 11.0 onwards

## Covered commits
- `88decbf` `2025-11-22` `fix(build-macos): support macOS 11.0 onwards`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/Makefile`
- `M` `app/scripts/build_llama_macos.sh`

## What changed from what, why, and how
The commit corrected behavior in `README.md`, `app/Makefile`, `app/scripts/build_llama_macos.sh`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(build-macos): support macOS 11.0 onwards`.

Before this commit, the repository reflected the state immediately preceding `88decbf`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -208,6 +208,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
    export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$(brew --prefix)/share/pkgconfig:$PKG_CONFIG_PATH"
    ```
 4. **Clone the repository and submodules** (same commands as Linux).
+   > The macOS build pins `MACOSX_DEPLOYMENT_TARGET=11.0` so the Mach-O `LC_BUILD_VERSION` covers Apple Silicon and newer releases (including Sequoia). Raise or lower it (e.g., `export MACOSX_DEPLOYMENT_TARGET=15.0`) if you need a different floor.
 5. **Build the llama runtime (Metal-only on macOS)**
    ```bash
    ./app/scripts/build_llama_macos.sh
@@ -219,6 +220,15 @@ File categorization with local LLMs is completely free of charge. If you prefer
    make -j4
    sudo make install   # optional
    ```
+   > **Fix for the 1.1.0 macOS build:** That package shipped with `LC_BUILD_VERSION` set to macOS 26.0, which Sequoia blocks. If you still have that build, you can patch it in place:
+   > ```bash
+   > APP="/Applications/AI File Sorter.app"
+   > BIN="$APP/Contents/MacOS/aifilesorter"
+   > vtool -replace -set-build-version macos 11.0 11.0 -output "$BIN.patched" "$BIN" && mv "$BIN.patched" "$BIN"
+   > codesign --force --deep --sign - "$APP"
+   > xattr -d com.apple.quarantine "$APP" || true
+   > ```
+   > (`vtool` ships with the Xcode command line tools.) Future releases are built with the corrected deployment target.
 
 ### Windows
```

The excerpt is taken from the commit diff for `fix(build-macos): support macOS 11.0 onwards`. The most relevant surfaces are `README.md`, `app/Makefile`, `app/scripts/build_llama_macos.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
