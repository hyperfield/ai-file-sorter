# Summary

This series gave remote LLM runs a proper backoff and pacing layer. The app now parses provider errors more consistently, respects `Retry-After` style guidance when remote services rate-limit requests, and can optionally throttle outbound request pace to a configured requests-per-minute ceiling. The same batch also added automated coverage and a documented manual smoke path for issue #95 validation.

# Motivation

Remote categorization had two related failure modes:

- providers could rate-limit bursts without the app reacting intelligently
- even when no explicit rate-limit response was returned, users could overwhelm remote APIs with back-to-back requests on large folders

The result was avoidable churn, noisy failures, and inconsistent throughput across providers.

# Implementation

The app introduced a dedicated `RemoteApiError` helper for normalizing non-success HTTP responses, then threaded a remote-throttle callback through `CategorizationService` so remote requests can be spaced intentionally when a rate limit is configured. The pacing layer is disabled for local LLMs and only applies to remote providers.

```cpp
if (now < next_remote_request) {
    const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        next_remote_request - now).count();
    ...
    std::this_thread::sleep_for(std::min(remaining, std::chrono::milliseconds(250)));
}
```

This loop is the practical heart of the change: instead of firing remote requests as fast as the file loop advances, the service now waits deliberately between requests when pacing is enabled.

# Validation

Validation included:

- `tests/unit/test_remote_api_error.cpp`
- `tests/unit/test_whitelist_and_prompt.cpp` additions covering backoff/pacing behavior
- `TESTS.md` documentation for manual issue #95 smoke validation

# User-visible impact

Remote-model users should see fewer burst-related failures and more predictable pacing on large runs. When rate limits do happen, the app can wait and retry instead of failing immediately.

# Remaining caveats

Provider behavior still varies. Some services expose clear retry metadata and some do not, so the pacing layer improves resilience but cannot make every remote API behave uniformly.
