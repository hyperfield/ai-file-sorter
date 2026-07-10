# MainApp analysis extraction and follow-up fixes

Commits covered: `44e4bd3`, `dcdf328`, `6adb46f`

## Why this change was justified

`MainApp.cpp` had accumulated several unrelated responsibilities: analysis orchestration, runtime selection, UI-state synchronization, and test access seams. That made the file harder to reason about, harder to test, and more fragile during later feature work. The refactor in `44e4bd3` attacked that structural problem directly by extracting focused collaborators instead of growing `MainApp` further.

The two follow-up commits belong in the same chapter because they were direct stabilization work:

- `dcdf328` fixed a Qt hash API compatibility issue that surfaced while keeping the test/build matrix healthy.
- `6adb46f` fixed a real lifetime bug introduced by the refactor: queued callbacks were outliving a temporary coordinator object.

In other words, this was not just “move code around”. It was a change to architectural boundaries, followed by the minimal compatibility and lifetime corrections needed to make the new structure safe.

## What changed

The refactor extracted three main areas out of `MainApp`:

1. analysis orchestration
2. visual-runtime handling
3. window-state / settings binding

The resulting design is much closer to a textbook separation-of-concerns model: one class coordinates the analysis pipeline, one class manages visual-LLM runtime details, and one class synchronizes window controls with stored settings.

### Analysis flow moved behind `AnalysisCoordinator`

Instead of keeping a large, stateful analysis routine inside `MainApp`, the work is delegated to a dedicated coordinator.

```cpp
// MainApp now delegates the heavy workflow instead of embedding it inline.
AnalysisCoordinator coordinator(*this, settings, db_manager, categorization_service);
coordinator.perform_analysis();
```

The important idea here is not the exact syntax but the inversion of responsibility:

- before: `MainApp` owned both the UI and the low-level orchestration
- after: `MainApp` triggers analysis, while `AnalysisCoordinator` owns the workflow details

That made later fixes easier because the crash scope became much smaller and more obvious.

### Visual LLM runtime logic became reusable

Visual runtime setup was also separated so other UI surfaces could reuse the same logic instead of open-coding model/runtime decisions in multiple places.

```cpp
// Shared visual runtime helper used from multiple UI flows.
auto runtime = VisualLlmRuntime::create(settings, core_logger);
```

This is the kind of extraction that reduces duplicated policy. Once model-file checks, runtime selection, and fallback behavior live in one place, new dialogs and analysis paths can share the same rules.

### UI/state synchronization moved out of `MainApp`

Settings-to-widget and widget-to-settings glue code often looks harmless, but it expands quickly and obscures the real responsibilities of a window class.

```cpp
// Binder centralizes UI/settings synchronization logic.
window_state_binder_->bind();
```

The justification is straightforward:

- UI binding is a separate problem from categorization logic
- extracted binding code is easier to test in isolation
- `MainApp` becomes more readable because it stops being a storage location for every checkbox rule

## The follow-up compatibility fix

The test-side `QCryptographicHash` usage needed to match the Qt 6 API that was actually available in the build matrix.

```cpp
// The test now uses the compatible overload rather than relying on a deprecated/incompatible call shape.
const QByteArray digest = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
```

This was a small change, but it was necessary to keep the refactor from landing with broken CI or platform-specific test failures.

## The lifetime bug and why it happened

The most important stabilization commit in this group is `6adb46f`.

After the refactor, analysis completion/error handlers were queued back to the UI thread from a temporary coordinator object. The callbacks were still capturing the coordinator’s `this` pointer even though the object had already gone out of scope by the time the queued work actually ran.

The corrected pattern was to capture `MainApp*` instead of the temporary coordinator object:

```cpp
// The queued callback is tied to MainApp lifetime, not a temporary coordinator.
run_on_ui([main_app = main_app_, result = std::move(result)]() mutable {
    main_app->handle_analysis_finished(std::move(result));
});
```

Why this matters:

- queued Qt/UI callbacks are asynchronous by nature
- temporary helper objects are dangerous callback anchors unless their lifetime is explicit
- extracting code into a helper class often reveals hidden lifetime assumptions that were previously masked by the old monolith

This fix is a good example of how a refactor can improve structure and still require one careful pass for ownership correctness.

## Net effect

By the end of this commit group:

- `MainApp` became smaller and less responsibility-heavy
- analysis orchestration gained a dedicated abstraction
- visual runtime policy became reusable
- settings/UI binding became explicit
- test/build compatibility stayed intact
- the refactor-specific queued-callback crash was removed

This chapter is therefore best understood as one logical change: a structural refactor plus the exact follow-up fixes required to make the new architecture production-safe.
