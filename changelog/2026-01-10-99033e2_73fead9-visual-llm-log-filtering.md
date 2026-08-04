# 2026-01-10 - Visual LLM log filtering and dev-only output

This changelog captures the changes made to reduce noisy Visual LLM logging
in terminal sessions while still keeping progress updates intact.

## Motivation

When the app is launched from a terminal, mtmd (the Visual LLM pipeline used
by LLaVA) logs the full prompt text via `add_text:` debug lines. This is
noisy, exposes prompt content that users do not need to see, and makes the
terminal output harder to scan.

## Summary of changes

1. Intercept mtmd logs for the full `infer_text` call.
2. Filter out prompt log lines (the `add_text:` entries).
3. Only emit remaining Visual LLM log output when the developer prompt logging
   flag is enabled.

## Log interception scope

The log callback is now installed before `mtmd_tokenize`, so prompt logs are
captured and filtered instead of printing via the default logger.

```cpp
// Keep mtmd logs under our control for the full infer_text call.
struct LogGuard {
    explicit LogGuard(LlavaImageAnalyzer* self) {
        mtmd_helper_log_set(&LlavaImageAnalyzer::mtmd_log_callback, self);
    }
    ~LogGuard() {
        mtmd_helper_log_set(nullptr, nullptr); // restore default logger
    }
};
```

## Prompt filtering

The log callback now skips any line that begins with `add_text:`, which is
the prefix mtmd uses when it dumps the prompt text.

```cpp
if (is_mtmd_prompt_log_line(text)) {
    return; // hide prompt content to avoid terminal spam
}
```

This preserves other useful mtmd logs while removing the prompt content.

## Developer-only output

Visual LLM log output is now gated behind the existing developer prompt
logging flag so it is off by default for normal users.

```cpp
vision_settings.log_visual_output = should_log_prompts();
```

This keeps diagnostic output available in developer mode without changing the
default user experience.

## Progress updates still work

Progress parsing remains unchanged: the callback still looks for the
`decoding image batch X/Y` lines and updates the UI progress regardless of
whether log output is enabled.

```cpp
if (self->settings_.batch_progress) {
    // parse "decoding image batch %d/%d" and update progress
}
```
