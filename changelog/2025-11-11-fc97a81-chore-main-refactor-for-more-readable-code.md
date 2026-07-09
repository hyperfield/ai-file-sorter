# 2025-11-11: chore(main): refactor for more readable code

## Covered commits
- `fc97a81` `2025-11-11` `chore(main): refactor for more readable code`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/scripts/build_llama_windows.ps1`

## What changed from what, why, and how
The commit reorganized or simplified code in `app/lib/LocalLLMClient.cpp`, `app/scripts/build_llama_windows.ps1`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `fc97a81`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -277,6 +277,73 @@ void reset_backend_memory_probe() {
 } // namespace TestHooks
 
 namespace {
+
+uint32_t read_le32(const char* ptr)
+{
+    uint32_t value = 0;
+    std::memcpy(&value, ptr, sizeof(uint32_t));
+    return value;
+}
+
+uint64_t read_le64(const char* ptr)
+{
+    uint64_t value = 0;
+    std::memcpy(&value, ptr, sizeof(uint64_t));
+    return value;
+}
+
+std::optional<int32_t> read_uint_value(uint32_t type,
+                                       const char* ptr,
+                                       std::size_t available_bytes)
+{
+    switch (type) {
+        case 4: // GGUF_TYPE_UINT32
+        case 5: // GGUF_TYPE_INT32:
+            if (available_bytes >= sizeof(uint32_t)) {
+                return static_cast<int32_t>(read_le32(ptr));
+            }
+            break;
+        case 10: // GGUF_TYPE_UINT64
+        case 11: // GGUF_TYPE_INT64
+            if (available_bytes >= sizeof(uint64_t)) {
+                return static_cast<int32_t>(read_le64(ptr));
+            }
+            break;
+        default:
+            break;
+    }
+    return std::nullopt;
+}
+
+std::optional<int32_t> parse_block_count_entry(const std::vector<char>& buffer,
+                                               std::size_t bytes_read,
+                                               std::size_t key_pos,
+                                               std::string_view key)
+{
+    if (key_pos < sizeof(uint64_t)) {
+        return std::nullopt;
+    }
+
+    const uint64_t declared_len = read_le64(buffer.data() + key_pos - sizeof(uint64_t));
+    if (declared_len != key.size()) {
+        return std::nullopt;
+    }
+
+    const std::size_t type_offset = key_pos + key.size();
+    if (type_offset + sizeof(uint32_t) > bytes_read) {
+        return std::nullopt;
+    }
+
+    const uint32_t type = read_le32(buffer.data() + type_offset);
+    const std::size_t value_offset = type_offset + sizeof(uint32_t);
+    if (value_offset >= bytes_read) {
+        return std::nullopt;
+    }
+
+    const std::size_t available = bytes_read - value_offset;
+    return read_uint_value(type, buffer.data() + value_offset, available);
+}
+
 std::optional<int32_t> extract_block_count(const std::string & model_path) {
     std::ifstream file(model_path, std::ios::binary);
     if (!file) {
@@ -304,79 +371,16 @@ std::optional<int32_t> extract_block_count(const std::string & model_path) {
     }
 
     const std::string_view data(buffer.data(), bytes_read);
-    static const std::string_view candidate_keys[] = {
+    [[maybe_unused]] static const std::string_view candidate_keys[] = {
         "llama.block_count",
         "llama.layer_count",
         "llama.n_layer",
     };
 
-    auto read_le32 = [](const char * ptr) -> uint32_t {
-        uint32_t value;
-        std::memcpy(&value, ptr, sizeof(uint32_t));
-        return value;
-    };
-
-    auto read_le64 = [](const char * ptr) -> uint64_t {
-        uint64_t value;
-        std::memcpy(&value, ptr, sizeof(uint64_t));
-        return value;
-    };
-
```

The excerpt is taken from the commit diff for `chore(main): refactor for more readable code`. The most relevant surfaces are `app/lib/LocalLLMClient.cpp`, `app/scripts/build_llama_windows.ps1`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
