# 2026-02-08: chore(documentation): update

## Covered commits
- `ffd4317` `2026-02-08` `chore(documentation): update`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `ffd4317`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -743,13 +743,18 @@ Follow the steps in [How to Use](#how-to-use), but modify **step 2** as follows:
 - Hugging Face: <https://huggingface.co>
 - JSONCPP: <https://github.com/open-source-parsers/jsoncpp>
 - LLaMa: <https://www.llama.com>
+- libzip: <https://libzip.org>
 - Local File Organizer <https://github.com/QiuYannnn/Local-File-Organizer>
 - llama.cpp <https://github.com/ggml-org/llama.cpp>
 - Mistral AI: <https://mistral.ai>
 - OpenAI: <https://platform.openai.com/docs/overview>
 - OpenSSL: <https://github.com/openssl/openssl>
+- PDFium: <https://pdfium.googlesource.com/pdfium/>
+- Poppler (pdftotext): <https://poppler.freedesktop.org/>
+- pugixml: <https://pugixml.org>
 - Qt: <https://www.qt.io/>
 - spdlog: <https://github.com/gabime/spdlog>
+- unzip (Info-ZIP): <https://infozip.sourceforge.net/>
 
 ## License
```

The excerpt is taken from the commit diff for `chore(documentation): update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
