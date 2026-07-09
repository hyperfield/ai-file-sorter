# GGUF Download Integrity and Active Reload Guards

## Summary

Commit `2cd0e54` hardens the model-download path so incomplete or corrupted GGUF artifacts are not quietly treated as ready, and the selection dialog no longer blindly retargets the downloader while an active download is already in progress.

The change affects both text-model and visual-model download handling.

## Motivation

The app had started relying more heavily on local GGUF artifacts, especially for visual backends and built-in local models. That made two failure modes more visible:

- a partially downloaded or corrupt GGUF file could look “present” even though it was unusable
- switching selections during an active download could leave the UI and downloader out of sync about which artifact was being fetched

Those are particularly frustrating failures because they waste time and usually show up only after the user thinks the model is already available.

## Implementation

### Artifact validation before “complete”

The downloader now validates completed artifacts for:

- non-zero file size
- expected content length when known
- a valid `GGUF` header when the target is a GGUF artifact

```cpp
std::optional<std::string> LLMDownloader::validate_completed_artifact(
    const std::filesystem::path& candidate) const
{
    ...
    if (real_content_length > 0 && actual_size != real_content_length) {
        return describe_size_mismatch(candidate, real_content_length, actual_size);
    }

    const bool expects_gguf = is_gguf_file_path(std::filesystem::path(download_destination))
        || is_gguf_file_path(candidate);
    if (expects_gguf && !has_gguf_header(candidate)) {
        return std::string("Downloaded file is invalid or incomplete (expected GGUF header): ")
            + candidate.string();
    }
```

This matters because the app can now classify a bad local file as `Corrupt` instead of `Complete`.

### Persistent partial-download handling

The same commit formalized sidecar metadata and partial-download file handling:

- `.aifs.meta` stores URL/content-length hints
- `.part` stores the incomplete payload
- legacy incomplete downloads are migrated into the partial-download path when possible

That makes resume and validation behavior more deterministic across restarts.

### UI guard against active downloader retargeting

The selection dialog now avoids swapping a downloader to a new URL while a download is already active:

```cpp
if (!downloader) {
    downloader = std::make_unique<LLMDownloader>(env_url);
} else if (downloader->get_download_url() != env_url) {
    if (is_downloading.load()) {
        return;
    }
    downloader->set_download_url(env_url);
}
```

This is the “active retarget” guard: if the dialog is already downloading one artifact, it does not silently repoint the downloader to a different model URL mid-flight.

## Validation

The commit added and updated focused tests around:

- `tests/unit/test_llm_downloader.cpp`
- `tests/unit/test_llm_selection_dialog_visual.cpp`
- `tests/unit/test_visual_llm_runtime.cpp`

Those tests cover corrupt artifact detection, partial-download behavior, and the dialog/runtime consequences of bad GGUF files.

## User-visible impact

Users should now see clearer status for broken model downloads:

- corrupt GGUF files are flagged as invalid instead of “ready”
- the dialog explicitly tells users to delete and redownload those files
- partial downloads resume more cleanly
- switching model selections during an active download is less likely to confuse the downloader state

## Remaining caveats

The validation is intentionally lightweight. It checks header and size expectations, not deep semantic correctness of the full GGUF payload.

That is still a good tradeoff for the UI path: it catches the common truncation and corruption failures quickly without turning the downloader into a full model parser.
