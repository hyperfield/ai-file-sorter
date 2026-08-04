# Summary

This feature added structured-project protection to recursive scans. AI File Sorter can now detect several project-root layouts, skip the strong matches automatically during recursive traversal, and avoid false positives from loose Unicode filenames while checking suffix-based rules.

# Motivation

Recursive scanning is convenient for photo folders and general archives, but it is dangerous inside software projects, game projects, or asset-driven workspaces where moving individual files can break imports, metadata links, build scripts, or engine-relative paths. The app needed a guardrail before recursive scans were enabled more broadly.

# Implementation

The new `ProtectedProjectDetector` introduced declarative rules for common project roots such as Unity, Unreal, Godot, Git repositories, Node projects, Python projects, Rust crates, Go modules, Gradle projects, .NET solutions, Xcode projects, and Blender project folders. Strong matches are skipped automatically during recursive scans, while weaker signals like a lone `.blend` file are only reported conservatively.

```cpp
bool ProtectedProjectDetector::should_skip(const ProtectedProjectMatch& match)
{
    return match.strength == ProtectedProjectStrength::Strong;
}
```

That simple strength gate is what keeps the feature conservative: obvious project roots are protected automatically, but weak signals do not block sorting by themselves.

# Validation

Validation came from new targeted tests in:

- `tests/unit/test_protected_project_detector.cpp`
- `tests/unit/test_file_scanner.cpp`
- the Unicode loose-file regression added in the follow-up fix commit

The README was also updated so users know recursive scans intentionally skip recognized project roots.

# User-visible impact

Recursive scans are safer. Users are less likely to accidentally reorganize code repositories, engine projects, or asset-bound workspaces whose files depend on a stable project-relative layout.

# Remaining caveats

The built-in rules are intentionally opinionated and conservative. They cover several common project types, but they do not guarantee detection of every proprietary or home-grown workspace layout.
