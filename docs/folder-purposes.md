# Folder Purposes

Existing-folder sorting can use descriptions of what belongs in each folder.
Descriptions express the archive's organization; they do not replace content
analysis or impose a global file-extension rule.

## Format

Place `.aifs-folder-purposes.json` at the destination archive root:

```json
{
  "version": 1,
  "folders": {
    "Media/Presentations": "Slide decks and talks, including product launch presentations.",
    "Communications/Press": "Press releases, media statements and journalist outreach, not internal presentation decks.",
    "Clients/Acme": "Acme project deliverables, organized by client rather than document format."
  }
}
```

Keys are exact, case-sensitive paths relative to the archive root, with forward
slashes. Each value is a nonempty UTF-8 string. Describe intended use, useful
inclusions, and exclusions when neighboring folders could be confused. Only
existing catalog paths receive descriptions; the file cannot add destinations.
Descriptions of ancestors accompany child candidates as context, without making
those ancestors additional choices in a bounded comparison.

The metadata is optional. Archives without it still use content evidence and
their existing hierarchy. The app reads this documented format, not arbitrary
README files or test-only hint filenames. No plugin is required.

## Limits And Safety

- Maximum file size: 128 KiB; maximum descriptions: 500.
- Maximum description length: 768 UTF-8 bytes. Overlong content is rejected, not silently shortened.
- Absolute, traversal and nonliteral paths are rejected. Metadata-file symlinks and nonregular files are not read.
- Invalid metadata is ignored as a whole with a warning in the log.
- Descriptions are JSON-encoded as untrusted data in inference input. They cannot authorize creation, expand allowed paths, or change the response contract.
- Input budget checks retain complete descriptions and evidence, splitting candidate groups when necessary rather than truncating them.
- Description edits invalidate cached routes. Metadata is never queued as a source file, even when scanning hidden files.

## Selection

Routing supplies folder descriptions alongside content evidence and candidate
paths. Native existing-folder replies remain JSON constrained to actual offered
paths or an explicit no-match outcome. Descriptions guide the model's choice;
they do not guarantee that the selected destination is appropriate.

If the model abstains and subsequently proposes a folder that already exists,
the app validates and reuses that literal destination as an existing folder.
The model's creation flag never overrides actual folder existence. Structural
validation still applies, and unresolved/invalid decisions remain blocked from apply.
Additional purpose-comparison instructions and contradiction reconsideration
are experimental routing options and are disabled in normal use.
