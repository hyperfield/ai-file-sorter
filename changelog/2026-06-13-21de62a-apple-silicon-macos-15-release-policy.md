# Summary

This commit tightened the documented and scripted macOS release target to Apple Silicon on macOS 15 and later. It removed older Intel-oriented and lower-version assumptions from the release flow instead of leaving them half-supported.

# Motivation

The earlier macOS packaging flow had accumulated assumptions about older macOS versions and Intel variants that no longer matched the intended release target. Keeping those references in the README and scripts made the support story look broader than the release tooling was actually prepared to sustain.

# Implementation

The README, `app/Makefile`, and the macOS packaging scripts were updated so the release flow now speaks consistently about Apple Silicon plus a macOS 15 floor. This was mainly a policy-and-tooling alignment change rather than a change to runtime categorization logic.

# Validation

Validation was limited to script and documentation consistency checks. The value here was eliminating misleading support signals, not adding new runtime behavior.

# User-visible impact

macOS release expectations became clearer. Users looking at the build and packaging docs no longer see outdated Intel-oriented release hints that the project does not intend to maintain going forward.

# Remaining caveats

This commit narrows the supported release story; it does not add compatibility for older macOS versions. Any out-of-band success on older systems is incidental rather than part of the maintained release contract.
