# Categorization Behavior

This page summarizes the app's categorization model, especially the parts that
are easy to lose inside a large README.

## Destination sorting default

New profiles use **Use existing folder structure**. New-folder suggestions are
off by default. An existing saved sorting-mode choice is preserved, including
**Create category folders**.

## Two categorization modes

### More refined

- Favors the most semantically accurate category and subcategory for the
  specific file.
- Is less constrained by stable top-level families such as `Documents` or
  `Images` when a more topic-specific category fits better.
- Typical outcomes: `Security / PCI DSS`, `Manuals / Camera Guides`,
  `Wildlife / Lions`.

### More consistent

- Favors stable filesystem-oriented top-level buckets across similar files.
- Reuses recent similar results as consistency hints when the evidence is close.
- Typical outcomes: `Documents / PCI DSS`, `Documents / Camera Guides`,
  `Images / Lions`.

## Supported document and image handling

- Supported document files receive document-specific prompt guidance.
- Supported image files receive image-specific prompt guidance.
- In `More consistent`, those prompt paths strongly prefer stable top-level
  families.
- In `More refined`, the prompt guidance is deliberately looser so semantically
  better top-level categories can survive when no whitelist forces them back.

## Whitelists

- Whitelists can constrain top-level categories only, global subcategories, or
  category-specific branching subcategories.
- Smart branching lets each main category have its own allowed subcategories.
- Whitelists apply in both categorization modes.
- Narrow whitelists are easiest for smaller models, but large whitelists are
  reduced before prompting so the model sees a more relevant candidate set.
- Allowed labels take precedence over learned examples and label-style defaults.
  Their spelling and language are preserved as canonical identities.
- If no permitted pair fits, the file remains unassigned for review. An invalid
  answer is never replaced with the first allowed category or subcategory.
  Abstention is offered only for a nonempty active whitelist. With unrestricted
  category generation, the prompt asks for descriptive labels instead.
- A valid image-category choice is preserved even when `Images` is also allowed.
- The optional unconstrained consistency pass preserves active whitelist
  assignments rather than rewriting them.

## Category language behavior

- Freely generated category labels are chosen canonically in English first;
  explicit whitelist labels retain the user's language and spelling.
- The UI then shows category labels in the selected category language.
- Suggested filenames for supported rename flows are localized the same way.
- Translation preserves proper names, acronyms and identifiers and does not
  change the category's meaning or level of detail.

## Suggested filenames

Document and image names use concise descriptive words, with the selected word
limit and filename extension preserved. Names are not restricted to nouns only.
Completion requests follow the response format of their task; filename
translation requests plain text, while document analysis requests JSON. A legacy
JSON filename wrapper is unwrapped without adding its keys to the filename.

## Document evidence and context budgets

Document analysis extracts up to 8,000 bytes of useful UTF-8 text by default.
Office XML is streamed past style definitions before applying that text limit;
raw XML scanning is separately bounded at 64 MiB. Text is kept on UTF-8 boundaries.

Local models count the complete chat prompt with their native tokenizer and
reserve response tokens. Document requests can grow from the configured context
through 4,096 and 8,192 tokens, bounded by model metadata and successful context
allocation. Smaller batch buffers are tried before reducing context. These
document sizes do not change the context configured for later categorization.

If the bounded text still does not fit, the same document analyzer summarizes
token-fitted chunks and combines their summaries. Providers without local token
metadata can split after explicit input-limit errors. Cancellation and request
counts are bounded; when model summarization cannot produce usable output,
bounded extracted text remains available as evidence instead of using only the
filename. Other runtime/API errors retain the existing retry behavior.

Production and native document experiments use this same analyzer and client
budget policy. `AI_FILE_SORTER_DOCUMENT_MAX_CONTEXT_TOKENS` can lower the document
ceiling for experiments or constrained hardware; `AI_FILE_SORTER_CTX_TOKENS`
continues to set the ordinary context. The document-evidence cache version is
updated so old extraction failures are retried with the new reader.

## Cache and learned behavior

- The categorization cache stores past results, rename suggestions, and
  consistency-oriented state so reruns are faster and steadier.
- Learned behavior is separate from the normal cache and comes from review
  decisions that the user explicitly approved.
- The cache acts as local memory for consistency. It does not train or modify
  the underlying model.
- Whitelist caches are checked against the current constraints and selection
  policy. Older results and results from a different whitelist are reanalyzed;
  user-approved learning data is retained. Unassigned results are not cached as
  successful categorizations.
- Missing content evidence does not discard an existing category or destination.
  The app refreshes that evidence first and recategorizes only after successful
  content analysis. Failed decoding or text extraction is remembered for the
  unchanged source and analyzer settings; temporary runtime/API errors remain
  retryable. Changed files, changed analyzer settings, or clearing the
  categorization cache allow another content-analysis attempt.

## Practical reading of the modes

- Use `More refined` when you want the folder structure to follow subject matter
  closely.
- Use `More consistent` when you want the broad folder layout to remain steady
  across mixed batches and repeated runs.
