#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>

enum class LLMChoice {
    Unset,
    Remote_OpenAI,
    Remote_Gemini,
    Remote_Custom, ///< Custom OpenAI-compatible endpoint.
    Local_3b,
    Local_3b_legacy,
    Local_7b,
    Custom
};

inline bool is_remote_choice(LLMChoice choice) {
    return choice == LLMChoice::Remote_OpenAI
        || choice == LLMChoice::Remote_Gemini
        || choice == LLMChoice::Remote_Custom;
}

enum class FileType {File, Directory};

struct CategorizedFile {
    std::string file_path;
    std::string file_name;
    FileType type;
    std::string category;
    std::string subcategory;
    int taxonomy_id{0};
    bool from_cache{false};
    bool used_consistency_hints{false};
    std::string suggested_name;
    bool rename_only{false};
    bool rename_applied{false};
};

inline std::string to_string(FileType type) {
    switch (type) {
        case FileType::File: return "File";
        case FileType::Directory: return "Directory";
        default: return "Unknown";
    }
}

struct FileEntry {
    std::string full_path;
    std::string file_name;
    FileType type;
};

struct CustomLLM {
    std::string id;
    std::string name;
    std::string description;
    std::string path;
};

inline bool is_valid_custom_llm(const CustomLLM& entry) {
    return !entry.id.empty() && !entry.name.empty() && !entry.path.empty();
}

/**
 * @brief Defines a custom OpenAI-compatible API endpoint.
 */
struct CustomApiEndpoint {
    std::string id;
    std::string name;
    std::string description;
    std::string base_url;
    std::string api_key;
    std::string model;
};

/**
 * @brief Returns true when the custom endpoint has the required fields.
 */
inline bool is_valid_custom_api_endpoint(const CustomApiEndpoint& entry) {
    return !entry.id.empty()
        && !entry.name.empty()
        && !entry.base_url.empty()
        && !entry.model.empty();
}

enum class FileScanOptions {
    None        = 0,
    Files       = 1 << 0,   // 0001
    Directories = 1 << 1,   // 0010
    HiddenFiles = 1 << 2,   // 0100
    Recursive   = 1 << 3    // 1000
};

inline bool has_flag(FileScanOptions value, FileScanOptions flag) {
    return (static_cast<int>(value) & static_cast<int>(flag)) != 0;
}

inline FileScanOptions operator|(FileScanOptions a, FileScanOptions b) {
    return static_cast<FileScanOptions>(static_cast<int>(a) | static_cast<int>(b));
}

inline FileScanOptions operator&(FileScanOptions a, FileScanOptions b) {
    return static_cast<FileScanOptions>(static_cast<int>(a) & static_cast<int>(b));
}

inline FileScanOptions operator~(FileScanOptions a) {
    return static_cast<FileScanOptions>(~static_cast<int>(a));
}

struct cudaDeviceProp {
    size_t totalGlobalMem;
};

struct UserCharacteristic {
    std::string trait_name;
    std::string value;
    float confidence;  // 0.0 to 1.0
    std::string evidence;
    std::string timestamp;
};

struct FolderInsight {
    std::string folder_path;
    std::string description;
    std::string dominant_categories;
    int file_count;
    std::string last_analyzed;
    std::string usage_pattern;  // e.g., "work", "personal", "archive"
};

struct UserProfile {
    std::string user_id;
    std::vector<UserCharacteristic> characteristics;
    std::vector<FolderInsight> folder_insights;
    std::string created_at;
    std::string last_updated;
};

#endif
