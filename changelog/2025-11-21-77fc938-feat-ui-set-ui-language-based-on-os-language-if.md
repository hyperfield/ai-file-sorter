# 2025-11-21: feat(ui): set ui language based on OS language if available

## Covered commits
- `77fc938` `2025-11-21` `feat(ui): set ui language based on OS language if available`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/IniConfig.hpp`
- `M` `app/lib/IniConfig.cpp`
- `M` `app/lib/Settings.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/include/IniConfig.hpp`, `app/lib/IniConfig.cpp`, `app/lib/Settings.cpp`. It changed the project from not having the capability described by `feat(ui): set ui language based on OS language if available` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `77fc938`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/IniConfig.hpp b/app/include/IniConfig.hpp
--- a/app/include/IniConfig.hpp
+++ b/app/include/IniConfig.hpp
@@ -13,9 +13,10 @@ public:
     std::string getValue(const std::string &section, const std::string &key, const std::string &default_value = "") const;
     void setValue(const std::string &section, const std::string &key, const std::string &value);
     bool save(const std::string &filename) const;
+    bool hasValue(const std::string& section, const std::string& key) const;
 
 private:
     std::map<std::string, std::map<std::string, std::string>> data;
 };
 
-#endif
\ No newline at end of file
+#endif
diff --git a/app/lib/IniConfig.cpp b/app/lib/IniConfig.cpp
index 14ebf0f..cc5daeb 100644
--- a/app/lib/IniConfig.cpp
+++ b/app/lib/IniConfig.cpp
@@ -119,3 +119,13 @@ bool IniConfig::save(const std::string &filename) const
 
     return true;
 }
+
+bool IniConfig::hasValue(const std::string& section, const std::string& key) const
+{
+    const auto sec_it = data.find(section);
+    if (sec_it == data.end()) {
+        return false;
+    }
+    const auto key_it = sec_it->second.find(key);
+    return key_it != sec_it->second.end();
+}
```

The excerpt is taken from the commit diff for `feat(ui): set ui language based on OS language if available`. The most relevant surfaces are `app/include/IniConfig.hpp`, `app/lib/IniConfig.cpp`, `app/lib/Settings.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
