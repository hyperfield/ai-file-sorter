# 2026-03-22: Platform-specific updater feeds, Windows installer/live-test paths, and localized categorization hardening

## Covered commits
- `4a199e8` `2026-03-22` `feat(updater): add platform-specific feeds and windows installer updates`
- `4711a33` `2026-03-22` `feat(windows-updater): add live test mode and zip installer updates`
- `aa827a7` `2026-03-22` `feat(app): add Windows updater variants and improve localized categorization`
- `7bddad3` `2026-03-22` `fix(i18n): remove precision bias from local model descriptions`

## Motivation
The 1.7.3 cycle needed a real Windows updater story rather than a single generic update feed. At the same time, non-English categorization quality needed hardening so localized labels could remain user-friendly without destabilizing the canonical taxonomy and cache behavior.

## What changed
This grouped feature chapter covers platform-specific update feeds, Windows installer download/zip/live-test paths, dedicated Windows update variants, improved localized categorization flow, and the follow-up fix that removed a precision bias from local-model descriptions in the translated UI.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `4a199e8`
```diff
diff --git a/.github/workflows/build.yml b/.github/workflows/build.yml
--- a/.github/workflows/build.yml
+++ b/.github/workflows/build.yml
@@ -7,7 +7,7 @@ on:
     branches: [ main, dev ]
 
 jobs:
-  build:
+  build-linux:
     runs-on: ${{ matrix.os }}
     strategy:
       matrix:
@@ -39,3 +39,41 @@ jobs:
 
       - name: Run ctest
         run: cd build && ctest --output-on-failure
+
+  build-windows:
+    runs-on: windows-latest
+    steps:
+      - name: Check out repository
+        uses: actions/checkout@v4
+        with:
+          submodules: recursive
+
+      - name: Enable long paths
+        run: git config --system core.longpaths true
+
+      - name: Set up vcpkg
+        shell: pwsh
+        run: |
+          git clone https://github.com/microsoft/vcpkg.git "$env:GITHUB_WORKSPACE\vcpkg"
+          & "$env:GITHUB_WORKSPACE\vcpkg\bootstrap-vcpkg.bat"
+
+      - name: Build llama runtime
+        shell: pwsh
+        run: |
+          $vcpkgRoot = "$env:GITHUB_WORKSPACE\vcpkg"
+          & "$env:GITHUB_WORKSPACE\app\scripts\build_llama_windows.ps1" `
+            "cuda=off" `
+            "vulkan=off" `
+            "blas=off" `
+            "vcpkgroot=$vcpkgRoot"
+
+      - name: Build and test Windows app
+        shell: pwsh
+        run: |
+          $vcpkgRoot = "$env:GITHUB_WORKSPACE\vcpkg"
+          & "$env:GITHUB_WORKSPACE\app\build_windows.ps1" `
+            -Configuration Release `
+            -VcpkgRoot $vcpkgRoot `
+            -BuildTests `
+            -RunTests `
+            -Parallel 4
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `4711a33`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -652,6 +652,11 @@ Storage and updates:
 - `AI_FILE_SORTER_CONFIG_DIR` - override the base config directory (where `config.ini` lives).
 - `CATEGORIZATION_CACHE_FILE` - override the SQLite cache filename inside the config dir.
 - `UPDATE_SPEC_FILE_URL` - override the update feed spec URL (dev/testing). The updater now reads per-platform streams from `update.windows`, `update.macos`, and `update.linux`, with legacy single-stream feeds still accepted.
+- `AI_FILE_SORTER_UPDATER_TEST_MODE` - enable Windows updater live-test mode (`1`/`true`). When enabled, the app skips the update feed fetch and synthesizes a newer version from the values below.
+- `AI_FILE_SORTER_UPDATER_TEST_URL` - direct URL for the Windows updater live-test package. This can point to an `.exe`, `.msi`, or a `.zip` containing exactly one `.exe` or `.msi`.
+- `AI_FILE_SORTER_UPDATER_TEST_SHA256` - SHA-256 checksum for the downloaded live-test package. If the URL points to a ZIP, this checksum must be for the ZIP archive itself.
+- `AI_FILE_SORTER_UPDATER_TEST_VERSION` - optional synthetic version shown by live-test mode. Defaults to the current app version with an extra trailing segment, for example `1.7.2.1`.
+- `AI_FILE_SORTER_UPDATER_TEST_MIN_VERSION` - optional synthetic minimum version for live-test mode. Defaults to `0.0.0` so the test behaves like an optional update.
 
 Example update feed:
 
@@ -692,8 +697,41 @@ Windows-only direct installer updates:
 
 - `installer_url` - direct URL to the Windows installer package.
 - `installer_sha256` - SHA-256 checksum used to verify the downloaded installer before launch.
+- `installer_url` can now also point to a ZIP archive, as long as the archive contains exactly one installer payload (`.exe` or `.msi`).
 - When both fields are present on Windows, the app can download the installer, verify it, and then prompt: `Quit the app and launch the installer to update`.
 
+Windows updater live-test mode:
+
+- `aifilesorter.exe` accepts the following flags directly on Windows:
+  `--updater-live-test`
+  `--updater-live-test-url=<https://.../AIFileSorterSetup.zip>`
+  `--updater-live-test-sha256=<sha256-of-the-downloaded-package>`
+  `--updater-live-test-version=<optional-version>`
+  `--updater-live-test-min-version=<optional-min-version>`
+- `StartAiFileSorter.exe` accepts and forwards the same flag family if you still use the bootstrapper path.
+- Live-test mode is Windows-only and intentionally bypasses the normal update JSON feed.
+- If the ZIP contains more than one `.exe` or `.msi`, the updater stops instead of guessing which installer to launch.
+- If `--updater-live-test` is present and the URL / SHA flags are omitted, `aifilesorter.exe` also looks for a `live-test.ini` file next to the executable and fills in the missing values from there.
+- Command-line flags still win over `live-test.ini`, so you can keep a default file and override just one field when needed.
+
+Example `live-test.ini`:
+
+```ini
+[LiveTest]
+download_url = https://files.example.com/AIFileSorterSetup-1.7.3.zip
+sha256 = 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
+current_version = 1.7.3
+min_version = 0.0.0
+```
+
+Example PowerShell launch:
+
+```powershell
+.\aifilesorter.exe `
+  --development `
+  --updater-live-test
+```
+
 ---
 
 ## Categorization cache database
```

This second excerpt is included because `4711a33` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
