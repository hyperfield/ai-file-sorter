# Storage-provider abstraction and cloud compatibility foundations

Commits covered: `e17f2b2`, `aca86d3`, `2e6f915`

## Why this change was justified

Before this work, the application treated the filesystem as a single concrete implementation detail. That was adequate for plain local folders, but it created a dead end for cloud-backed folders such as OneDrive, Dropbox, or pCloud. The app could not express questions such as:

- is this path hydrated or online-only?
- should reparse points be skipped?
- should undo validation be more forgiving for a synced provider?
- is a provider-specific compatibility mode available?

The correct architectural move was to introduce an abstraction layer before building provider-specific features. That happened in `e17f2b2`, and the next two commits extended it into actual cloud-awareness plus tests.

## Step 1: introduce a storage-provider contract

The central idea was to define an interface that abstracts storage operations the rest of the app cares about.

```cpp
class IStorageProvider {
public:
    virtual std::string id() const = 0;
    virtual StorageProviderDetection detect(const std::string& root_path) const = 0;
    virtual std::vector<FileEntry> list_directory(const std::string& directory,
                                                  FileScanOptions options) const = 0;
    virtual StorageMutationResult move_entry(const std::string& source,
                                             const std::string& destination) const = 0;
    virtual StorageMutationResult undo_move(const std::string& source,
                                            const std::string& destination) const = 0;
};
```

This is the critical architectural pivot:

- before: storage logic was implicitly “local filesystem logic”
- after: storage logic became a contract with pluggable implementations

The first concrete provider was naturally the local filesystem provider, which preserved existing behavior while giving the rest of the app something stable to depend on.

## Step 2: route scan, move, and undo through the abstraction

The initial abstraction was not theoretical. `ResultsCoordinator`, `CategorizationDialog`, `MovableCategorizedFile`, and `UndoManager` were moved over to provider-backed behavior.

That mattered because it pushed the abstraction into real application paths:

- scan and cache logic
- move application logic
- undo persistence and replay

Once those flows depend on `IStorageProvider`, cloud-specific behavior no longer needs invasive patches across unrelated UI code.

### Example: move/undo state now stores provider metadata

```cpp
// Undo state keeps track of which provider owns the move history.
undo_record.provider_id = provider->id();
undo_record.metadata = mutation_result.metadata;
```

That is what enables later commits to make OneDrive undo different from plain local undo without redesigning the whole undo layer again.

## Step 3: add cloud compatibility providers and plugin loading

With the provider seam in place, `2e6f915` added the first cloud-aware provider types plus plugin-management scaffolding.

This included:

- `CloudCompatibilityProvider`
- `CloudPathDetectorProvider`
- `CloudPathSupport`
- plugin manifest / manager / loader classes
- plugin dialog UI

The cloud support at this stage was intentionally compatibility-oriented rather than fully provider-native.

```cpp
// Heuristic detector provider used to recognize likely cloud-backed folders.
StorageProviderDetection detection;
detection.provider_id = "dropbox";
detection.matched = true;
detection.needs_additional_support = true;
```

The goal here was to separate three concerns:

1. detect a likely cloud-backed folder
2. decide whether additional support is needed
3. allow the app to switch providers once support exists

That is much more extensible than hard-coding special-case path logic in the main window.

## Why the tests belong in this chapter

`aca86d3` is a pure test commit, but it belongs in the same logical chapter because it validates the provider seam where it matters most:

- scan behavior
- undo behavior
- dialog integration

This is exactly the kind of follow-up that should be grouped with a feature chapter: it does not introduce a new product capability, but it makes the new abstraction trustworthy.

```cpp
// Example test intent: storage-backed scan/undo behavior remains correct
REQUIRE(results_coordinator.find_files_to_categorize(...).size() == expected_count);
CHECK(undo_manager.restore_last_plan(...));
```

## Why this was the right order of implementation

The order mattered:

1. first create an abstraction for scan/move/undo
2. then add cloud-compatible detection/providers
3. then build plugin/runtime support on top of that

If cloud support had been added first without a provider seam, later plugin work would have required a much more invasive rewrite.

## Net effect

By the end of this group:

- the app no longer depended exclusively on direct local-filesystem behavior
- provider-aware scan, move, and undo flows existed
- cloud folder detection had a real architectural home
- plugin loading/management had an initial foundation
- tests covered the new seam where regressions would otherwise be easy

This chapter is therefore the real starting point of storage extensibility in AI File Sorter. Later OneDrive/plugin work depends on this foundation.
