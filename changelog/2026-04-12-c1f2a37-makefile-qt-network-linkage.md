# Makefile QtNetwork linkage for single-instance support

Commits covered: `c1f2a37`

## Summary

This commit fixed the non-CMake build path after single-instance support was added. The Makefile-based build was still linking only `Qt6Core`, `Qt6Gui`, and `Qt6Widgets`, but the new coordinator uses `QLocalServer` and `QLocalSocket`, which live in `Qt6Network`.

## Motivation

The CMake build had already been updated to link `Qt6::Network`, but the hand-maintained Makefile path lagged behind. That produced the exact kind of split-brain build failure that happens when one build system learns about a new module and the other does not.

## Implementation

The fix was deliberately small:

- `Qt6Network` was added to `QT_PACKAGES` on Linux and macOS in `app/Makefile`
- the fallback include and link flags gained `QtNetwork` / `-lQt6Network`
- `SingleInstanceCoordinator.cpp` switched to explicit module-qualified includes:

```cpp
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
```

That makes the dependency explicit and keeps the source aligned with the module that actually owns those classes.

## Validation

Validation for this change is build-oriented. The point of the commit was to make the Makefile path compile again after the single-instance feature landed.

## User-visible impact

There is no new end-user feature here. The practical effect is that developers building through `make` do not hit missing-QtNetwork failures when compiling the single-instance coordinator.

## Remaining caveats

- This commit only fixes the Makefile path. CMake already had the correct dependency at this point.
