# UTF-8 filesystem path handling in analysis

Commit covered: `41c9774`

## Why this change was justified

The application already handled many Unicode-facing UI cases correctly, but several analysis and progress-reporting paths still relied on path conversions that were fragile across platforms. That is especially dangerous in a file-sorting application, because filenames and directory names are not “edge cases”; they are the core input domain.

When path handling is even slightly inconsistent, the user can experience one or more of the following:

- files silently skipped because lookup keys do not match
- broken progress output for non-ASCII paths
- analysis tools receiving garbled paths
- different behavior between Linux, Windows, and macOS

This commit hardened the analysis-related path flow so UTF-8 paths are carried consistently where the app expects UTF-8 strings.

## What changed

The work touched the analysis coordinator, entry router, progress dialog, and both document/image analyzers.

The essential change pattern looked like this:

```cpp
// Prefer a UTF-8 string representation when passing a path into analysis code.
const std::string utf8_path = path.u8string();
```

The motivation is not cosmetic. The application already uses `std::string` extensively in analysis and persistence code, so if those strings are expected to be UTF-8, the path conversion must make that expectation explicit rather than relying on locale-dependent or platform-dependent conversions.

## Why analysis code was the right place to fix it

It is tempting to treat encoding problems as a utility-layer concern only, but in practice analysis code is where filenames are:

- logged
- passed to model prompts
- used as cache or routing inputs
- shown in progress status

That means an encoding fix limited to one helper is often incomplete. The affected code paths needed to be aligned end-to-end.

### Example: progress reporting

The progress dialog must describe the file currently being processed. If the conversion path is inconsistent, users see mojibake or truncated names exactly when they are trying to understand what the app is doing.

```cpp
// Progress output now uses a UTF-8-safe path string.
append_log_line(QString::fromUtf8(current_path.c_str()));
```

### Example: document and image analyzers

Analyzers need the real path to open files, feed prompt context, and produce traceable diagnostics.

```cpp
// Analyzer-side path handoff should preserve the original path bytes as UTF-8 text.
request.file_path = normalized_utf8_path;
```

The principle is simple:

- path handling should be explicit
- UI-facing strings should be decoded as UTF-8
- analysis/persistence code should not depend on the ambient locale

## Why this matters beyond “internationalization”

This was not only a localization improvement. It also improved correctness and reproducibility.

In a file sorter, “same file, different string representation” can easily become:

- cache misses
- duplicate work
- broken routing
- misleading logs

The safer UTF-8 path flow therefore improves both usability and internal consistency.

## Net effect

After this change:

- non-ASCII paths are handled more consistently through the analysis pipeline
- progress views present filenames more reliably
- document and image analyzers receive stable UTF-8 path strings
- the app is less dependent on platform-local encoding behavior

This was a narrow change, but a foundational one for a program that is defined by how it interacts with real user filenames.
