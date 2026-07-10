# Named Constants for Runtime Thresholds

## Summary

Commit `fec475c` replaces a spread of ad-hoc numeric literals with named constants across core runtime paths. The change touches local LLM loading, document analysis, support-code management, consistency passes, analysis coordination, and utility formatting.

The practical outcome is not a new feature. It is a maintenance and correctness pass that makes runtime policies easier to understand and tune, and it fixes a small formatting bug for byte values below one kilobyte.

## Motivation

Several subsystems had policy values embedded directly in code:

- retry counts and memory thresholds in local LLM handling
- token and timeout limits in document and categorization helpers
- unit-conversion values in utility formatting

That made the code harder to review and easy to misread. It also hid one concrete bug: values smaller than `1024` bytes were still being divided by `1024` while labeled as bytes.

## Implementation

The commit promotes thresholds and conversion values to local `constexpr` names near their usage instead of leaving them inline. The intent is easiest to see in the utility size formatter:

```cpp
constexpr double kBytesPerKilobyte = 1024.0;
constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;

if (bytes < kBytesPerKilobyte) {
    return QString::asprintf("%.2f B", static_cast<double>(bytes));
}
```

This matters because the code now distinguishes the raw-byte case from the kilobyte-and-up cases instead of applying one conversion path to all inputs.

The same cleanup pattern was applied in:

- `LocalLLMClient.cpp` for backend estimation and retry-related values
- `AnalysisCoordinator.cpp` for analysis workflow policy constants
- `ConsistencyPassService.cpp` for prompt and pass settings
- `DocumentTextAnalyzer.*` for document-analysis defaults
- `SupportCodeManager.cpp` for short support-code policy values

## Validation

During implementation, the following targeted validation was run:

```bash
cmake --build build-tests --target ai_file_sorter_tests -j4
cmake --build build-tests --target aifilesorter -j4
./build-tests/ai_file_sorter_tests "format_size keeps byte values in bytes below one kilobyte"
./build-tests/ai_file_sorter_tests "LlavaImageAnalyzer*"
./build-tests/ai_file_sorter_tests "DocumentTextAnalyzer*"
```

The most direct regression coverage added by this commit is the byte-formatting test that locks in correct behavior below `1 KB`.

## User-visible impact

Most of the change is internal. The only clear user-visible effect is that tiny file sizes now format correctly as bytes instead of showing an incorrectly scaled byte label.

The larger benefit is operational: later runtime tuning work can reference named policy constants instead of repeating unexplained numbers.

## Remaining caveats

This commit improves readability and local correctness, but it does not by itself redesign the underlying heuristics. If a threshold is still too conservative or too aggressive, the constant is now easier to find and change, not automatically correct.
