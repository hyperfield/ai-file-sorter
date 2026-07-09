# OneDrive plugin runtime and authoritative detection

Commits covered: `3affdf2`, `fd87e2d`

## Why this change was justified

The earlier cloud-compatibility layer established that the app could detect cloud-backed folders and route operations through provider abstractions. But that was still a generic compatibility story. OneDrive needed something stronger:

- a dedicated runtime
- richer move/undo metadata
- provider-owned status inspection
- a clear distinction between “plugin installed”, “plugin installable”, and “plugin unavailable”

That is what these two commits delivered. The first introduced the dedicated OneDrive plugin/runtime and update-aware plugin management. The second made detection more authoritative and pushed the UI toward explicit storage-support states instead of a vague “cloud folder detected” path.

## Step 1: a dedicated OneDrive runtime plugin

The OneDrive work stopped treating all cloud-backed folders as one generic bucket and introduced a provider/runtime with its own implementation and external connector process.

```cpp
// The loader can now build a dedicated OneDrive provider runtime.
if (manifest.id == "onedrive_storage_support") {
    return create_onedrive_provider(manifest);
}
```

This was justified because OneDrive behavior differs materially from plain local storage and from other sync providers:

- hydration / online-only states
- sync-lock / staging behavior
- richer identity and revision tracking
- platform-specific authoritative detection on Windows

Those concerns warranted a dedicated runtime instead of more conditionals in a generic provider.

## The plugin system became real, not merely architectural

This commit group is also where the storage plugin story became operational:

- plugin manifests gained entry-point semantics
- external-process providers became real
- archives and remote catalogs became install/update sources
- the UI could manage plugin lifecycle explicitly

```cpp
// External-process plugin providers are created from a manifest entry point.
manifest.entry_point_kind = "external_process";
manifest.entry_point = "aifs_onedrive_storage_plugin";
```

That matters because it decouples plugin runtime implementation from the main app process while still letting the app manage discovery, installation, and switching.

## Step 2: OneDrive-specific detection and state handling

The second commit in this chapter raised the quality of detection. Instead of only asking “does the path look like OneDrive?”, the provider can prefer authoritative sync-root information when the platform exposes it.

```cpp
StorageProviderDetection detection;
detection.provider_id = "onedrive";
detection.matched = true;
detection.detection_source = "windows_sync_root";
```

This is important conceptually:

- heuristic detection is a useful fallback
- authoritative sync-root detection is stronger evidence
- the app can now tell the difference and act accordingly

That is what allows the UI to say more precise things about why a provider was selected or why native plugin support is unavailable.

## Explicit support states in `MainApp`

The app also gained a better model for cloud-folder handling:

- detected and supported via plugin
- detected but plugin not installed
- detected but no plugin exists

```cpp
if (plugin_installed) {
    return StorageSupportState::detected_and_supported_via_plugin;
}
if (plugin_exists) {
    return StorageSupportState::detected_but_plugin_not_installed;
}
return StorageSupportState::detected_but_no_plugin_exists;
```

This is one of the most important user-facing improvements in the storage work. The app is no longer limited to a binary “cloud / not cloud” decision. It can now present a more truthful explanation of what support actually exists.

## Why this chapter groups these two commits

Although `3affdf2` and `fd87e2d` are separate commits, they are one coherent feature narrative:

- `3affdf2` created the OneDrive runtime/plugin machinery
- `fd87e2d` made detection and UI state resolution precise enough to use that machinery well

Grouping them is justified because the later commit is not an unrelated feature. It is the missing precision layer that turns “we have a OneDrive plugin” into “the app can reason correctly about when and how to use it”.

## Representative code excerpts

### Provider-aware mutation metadata

```cpp
// Provider-owned move results capture metadata used later for undo validation.
StorageMutationResult result;
result.success = true;
result.metadata.stable_identity = computed_identity;
result.metadata.revision_token = computed_revision;
```

This is the key to stronger undo safety: the move path returns provider metadata that later commits can validate against.

### Authoritative detection beats heuristics

```cpp
// If the OS gives authoritative sync-root info, prefer it over path guessing.
if (sync_root && contains_case_insensitive(sync_root->provider_name, "onedrive")) {
    detection.matched = true;
    detection.confidence = 100;
    detection.detection_source = "windows_sync_root";
}
```

The design principle is textbook-quality: use stronger evidence first, keep weaker heuristics only as fallback.

## Net effect

By the end of this chapter:

- OneDrive had become a first-class plugin/runtime, not just a compatibility note
- the plugin system could install and update real provider runtimes
- detection could become authoritative instead of purely heuristic
- `MainApp` could explain plugin support state more precisely

This was the point where storage plugin support stopped being abstract scaffolding and became a real product capability.
