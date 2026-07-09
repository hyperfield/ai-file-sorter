# Loaded-backend status in the main window

Commits covered: `f81e7b1`

## Summary

This UI-focused commit added a persistent backend-status label to the main window and fixed the LLM selection dialog so it reliably uses the app icon even when the dialog is opened without a parent window already supplying one.

## Motivation

The app supported multiple backend families by this point:

- local CPU / BLAS
- local GPU backends such as CUDA, Vulkan, and Metal
- OpenAI
- Gemini
- custom OpenAI-compatible APIs

But once the user had made a choice, the active backend was not surfaced clearly in the main window. That made fallback behavior, especially GPU-to-CPU fallback, harder to understand. The dialog icon problem was smaller but visible polish debt.

## Implementation

The status bar now hosts a permanent label, and the text is computed from the active LLM choice plus local backend detection:

```cpp
if (backend_key == "cuda" || backend_key == "vulkan" ||
    backend_key == "metal" || backend_key == "mtl") {
    return tr("Loaded GPU backend: %1 with %2").arg(backend_name, cpu_backend);
}
```

That means the label can distinguish between:

- OpenAI / Gemini / Custom API
- plain CPU
- CPU with a specific BLAS backend
- GPU backend plus the underlying CPU/BLAS label

The label is refreshed:

- after UI translation
- after the LLM selection dialog is accepted
- after local and remote client creation
- after local GPU fallback events

The dialog icon fix is simpler but important: `LLMSelectionDialog` now first asks `QApplication::windowIcon()` and falls back to bundled icon resources if needed.

## Validation

No new dedicated automated UI test was added here. Validation is mostly manual UI behavior:

- the status label updates when backend choice changes
- the label survives translations
- the dialog no longer appears without an icon when launched outside the usual parent-window path

## User-visible impact

Users can now tell, at a glance, which backend is actually loaded. That is especially useful after local fallback events, where the app may silently continue on CPU but now also reflects that state in the main window.

## Remaining caveats

- The label is based on runtime/environment inference rather than deep model introspection, so it reports the backend family rather than every implementation detail.
- This is UI clarity, not a change to backend selection itself.
