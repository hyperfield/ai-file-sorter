# 2026-03-06: Recategorization cache reset, support-code compatibility, and trademark policy

## Scope

This entry documents the set of changes shipped on 2026-03-06:

- `f3d186c` `fix(app): reset stale cache on recategorization and harden donation link launch`
- `0f39692` `fix(support-code): use Qt-compatible hash API and rotate verification key`
- `cd5d8f9` `chore(deps): bump llama.cpp and Catch2 submodule revisions`
- `ee97663` `docs(legal): add project trademark policy notice`

The changes were intentionally grouped around two user-visible reliability issues and two maintenance/legal follow-ups.

## Motivation

### 1) Recategorization had stale-cache behavior in recursive mode

When users switched categorization style and requested recategorization, mixed-style cache rows could survive in nested directories. The practical outcome was inconsistent behavior:

- a folder appeared "recategorized", but
- nested rows from old mode could still be reused,
- causing unexpected cached dialogs/results.

The cache reset logic needed subtree-aware delete and subtree-aware conflict detection.

### 2) Donation browser launch had a silent failure path

Users could click support/donation actions and still not get a browser tab, with no immediate recovery path. We needed:

- a stronger Linux fallback (`xdg-open`) and
- a manual URL fallback dialog when automatic launch fails.

### 3) Qt6 build portability regression on Ubuntu

`QCryptographicHash::addData(QByteArrayView)` was not available in the target Ubuntu Qt package variant, while macOS builds passed. The code needed to use the stable pointer+length overload to keep cross-platform builds aligned.

### 4) Website signer key changed

The app verifier public key had to be rotated to match the website-side signer key used for donation code issuance.

### 5) Dependency hygiene and mark usage clarity

- Submodules were updated (`llama.cpp`, `Catch2`) to track upstream fixes/compatibility.
- `TRADEMARKS.md` was added to explicitly separate AGPL code rights from project mark usage rights.

## Detailed changes

### A. Recursive cache reset and conflict detection

Before:

- `clear_directory_categorizations(dir)` only removed exact `dir_path = ?`.
- style checks looked at one folder style snapshot and could miss nested mixed cache rows.

After:

- cache clearing supports optional recursive subtree deletion via `dir_path LIKE pattern`.
- new `has_categorization_style_conflict(dir, desired_style, recursive)` detects any mismatch row in subtree.
- `MainApp::ensure_folder_categorization_style` now uses subtree conflict detection and clears recursively when recursive scan is enabled.

#### Excerpt: subtree-aware delete and conflict query

```cpp
bool DatabaseManager::clear_directory_categorizations(const std::string& dir_path,
                                                      bool recursive) {
    const char* sql = recursive
        ? "DELETE FROM file_categorization WHERE dir_path = ? OR dir_path LIKE ? ESCAPE '\\';"
        : "DELETE FROM file_categorization WHERE dir_path = ?;";
    // Comment: recursive mode now removes nested cache rows, not only exact folder rows.
}

bool DatabaseManager::has_categorization_style_conflict(const std::string& dir_path,
                                                        bool desired_style,
                                                        bool recursive) const {
    const char* sql = recursive
        ? "SELECT 1 FROM file_categorization "
          "WHERE (dir_path = ? OR dir_path LIKE ? ESCAPE '\\') "
          "AND IFNULL(categorization_style, 0) != ? LIMIT 1;"
        : "SELECT 1 FROM file_categorization "
          "WHERE dir_path = ? AND IFNULL(categorization_style, 0) != ? LIMIT 1;";
    // Comment: conflict check now evaluates any old-style row in the active scope.
}
```

Source locations:

- `app/lib/DatabaseManager.cpp`
- `app/include/DatabaseManager.hpp`

#### Excerpt: caller now honors recursive mode

```cpp
const bool desired = settings.get_use_consistency_hints();
const bool recursive = settings.get_include_subdirectories();
if (!db_manager.has_categorization_style_conflict(folder_path, desired, recursive)) {
    return true; // Comment: no mixed-style rows in relevant scope, no reset needed.
}

if (box.clickedButton() == recategorize_button) {
    if (!db_manager.clear_directory_categorizations(folder_path, recursive)) {
        show_error_dialog(...);
        return false;
    }
}
```

Source location:

- `app/lib/MainApp.cpp`

#### Excerpt: regression test for stale subtree rows

```cpp
// Simulate a partially re-categorized subtree:
// root row uses new style, nested row still uses old style.
REQUIRE(db.insert_or_update_file_with_categorization("root.txt", ..., true, ...));
REQUIRE(db.insert_or_update_file_with_categorization("child.txt", ..., false, ...));

CHECK(db.has_categorization_style_conflict(root_path, true, true));
REQUIRE(db.clear_directory_categorizations(root_path, true));
CHECK(service.load_cached_entries(root_path).empty());
```

Source location:

- `tests/unit/test_cache_interactions.cpp`

### B. Donation URL launch hardening

Before:

- `open_support_page()` returned `void` and used only `QDesktopServices::openUrl`.
- failure to open had no explicit recovery path.

After:

- `open_support_page()` returns `bool`.
- Linux fallback calls `xdg-open` through detached `QProcess`.
- support prompt shows manual URL instruction if open fails.

#### Excerpt: cross-platform open fallback

```cpp
bool open_external_url(const QUrl& url)
{
    if (QDesktopServices::openUrl(url)) {
        return true; // Comment: preferred desktop abstraction path.
    }
#if defined(Q_OS_LINUX)
    return QProcess::startDetached("xdg-open", {url.toString(QUrl::FullyEncoded)});
    // Comment: Linux fallback when Qt desktop handler fails.
#else
    return false;
#endif
}
```

Source location:

- `app/lib/MainAppHelpActions.cpp`

#### Excerpt: user-visible manual fallback in prompt flow

```cpp
if (!MainAppHelpActions::open_support_page()) {
    QMessageBox::information(
        this,
        tr("Open donation page"),
        tr("Could not open your browser automatically.\nPlease open this link manually:\n%1")
            .arg(QString::fromUtf8(kDonationUrl)));
    // Comment: user always gets a path forward even when auto-open fails.
}
```

Source location:

- `app/lib/MainApp.cpp`

### C. Qt-compatible hash API and key rotation

Before:

- code used `hash.addData(QByteArrayView(...))`, which failed in target Ubuntu Qt headers.
- verifier key no longer matched website signer key.

After:

- switched to stable pointer+length overload:
  - `hash.addData(label.data(), size)`
  - `hash.addData(reinterpret_cast<const char*>(...), size)`
- updated `kVerificationPublicKey` bytes to the new signer pair.

#### Excerpt: hash API compatibility fix

```cpp
QCryptographicHash hash(QCryptographicHash::Sha256);
hash.addData(label.data(), static_cast<qsizetype>(label.size()));
hash.addData(reinterpret_cast<const char*>(kBlobPepper.data()),
             static_cast<qsizetype>(kBlobPepper.size()));
// Comment: avoids QByteArrayView-only overload dependency differences across Qt6 builds.
```

Source location:

- `app/lib/SupportCodeManager.cpp`

## Maintenance changes

### Dependency submodule revisions

- `app/include/external/llama.cpp` updated to `ae9f8df...`
- `external/Catch2` updated to `b59f4f3...`

These are pointer bumps only; behavior changes derive from the pinned upstream revisions.

### Trademark policy file

`TRADEMARKS.md` was added to document:

- AGPL licensing scope for code/copyright material.
- reserved rights for project marks, names, and branding assets.
- expected behavior for forks/modified redistributions.

## Outcome and risk profile

### User-facing outcomes

- Recursive recategorization now reliably clears stale nested cache rows when mode changes.
- Support/donation action now has robust browser-launch fallback and manual escape hatch.
- Ubuntu Qt6 builds no longer fail on the `QCryptographicHash::addData` signature mismatch.
- Donation code verification aligns with current website-side key material.

### Risk notes

- Recursive delete uses pattern matching and is intentionally broader in scope than exact-folder delete; this is required for subtree reset correctness.
- Key rotation is a hard compatibility boundary: old signatures will not validate against the new key by design.
- Submodule bumps may bring transitive behavior changes from upstream; pin hashes are recorded above for reproducibility.

