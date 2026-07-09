# Visual Unavailability and Progress Filtering

## Summary

Commits `a265909` and `34beee4` made the image-analysis path degrade more gracefully when the visual runtime is unavailable:

- users can continue a run using filenames only instead of treating visual analysis failure as an all-or-nothing stop
- noisy visual diagnostics are hidden from normal users and shown only in development or test modes

## Motivation

Image analysis now depends on a more complex local visual stack with downloadable GGUF artifacts and backend-specific runtime conditions. When that stack is missing, corrupt, or temporarily unusable, the app still has a meaningful fallback: continue with filename-based categorization and renaming behavior where possible.

At the same time, the progress dialog had started surfacing low-level vision runtime/timing messages that were useful for debugging but distracting for ordinary users.

## Implementation

### Continue-without-visual-analysis flow

When visual analysis cannot run, the main window now offers a specific continuation choice:

```cpp
box.setWindowTitle(tr("Continue without visual analysis?"));
box.setText(tr("Image analysis is unavailable."));
box.setInformativeText(
    tr("Continue this analysis using filenames only? Cancel will stop this analysis."));
```

If the user accepts, the run continues using the non-visual path. If the user declines, the analysis is cancelled explicitly instead of drifting into a confusing partial state.

### GGUF artifact validation

The visual path also gained lightweight artifact validation helpers:

```cpp
inline bool has_gguf_header(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }

    std::array<char, 4> magic{};
    stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    return stream.gcount() == static_cast<std::streamsize>(magic.size())
        && std::string_view(magic.data(), magic.size()) == "GGUF";
}
```

That check gives the app a fast way to distinguish “artifact missing or corrupt” from ordinary runtime failures before the visual backend gets far into initialization.

### Development-only progress diagnostics

The later follow-up commit hides low-level visual timing/runtime messages unless the app is in development or test mode:

```cpp
const bool vision_diagnostic =
    message.rfind("[VISION] Runtime: ", 0) == 0 ||
    message.rfind("[VISION] Timing ", 0) == 0;
if (!vision_diagnostic) {
    return true;
}

return is_development_mode() || is_test_mode();
```

This preserves useful diagnostics for debugging without turning the normal progress dialog into a developer console.

## Validation

The main validation for this sequence came from:

- `tests/unit/test_main_app_visual_fallback.cpp`
- `tests/unit/test_image_analyzer_factory.cpp`

Those tests cover both the continuation/cancellation behavior and the development-only visibility rules for the vision diagnostics.

## User-visible impact

Users now get a much clearer experience when image analysis cannot run:

- missing or corrupt visual artifacts no longer force a confusing hard failure
- the app can continue without visual analysis when that still makes sense
- the progress dialog is quieter in normal use

The result is a more resilient mixed-file analysis flow, especially on machines where the visual backend is optional or still being set up.

## Remaining caveats

Continuing without visual analysis is a compromise, not a perfect substitute. Image-specific naming and content-aware categorization will still be weaker when the run falls back to filename-only handling.

The diagnostics are hidden only in the progress dialog. Detailed logging and test/development visibility remain important for debugging runtime-specific visual problems.
