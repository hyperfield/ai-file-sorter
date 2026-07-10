## Changes
- Centralized `CustomLLM` validation by adding `is_valid_custom_llm` next to the struct definition so all callers share a single rule.
- `Settings` now relies on the shared validator when persisting custom LLM entries.

## Code Excerpts
```cpp
// app/include/Types.hpp
struct CustomLLM {
    std::string id;
    std::string name;
    std::string description;
    std::string path;
};

inline bool is_valid_custom_llm(const CustomLLM& entry) {
    return !entry.id.empty() && !entry.name.empty() && !entry.path.empty();
}
```

```cpp
// app/lib/Settings.cpp (save_custom_llms)
for (const auto& entry : custom_llms) {
    if (!is_valid_custom_llm(entry)) {
        continue;
    }
    ids.push_back(entry.id);
    const std::string section = "LLM_" + entry.id;
    config.setValue(section, "Name", entry.name);
    config.setValue(section, "Description", entry.description);
    config.setValue(section, "Path", entry.path);
}
```
