#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>

enum class LLMChoice {
    Unset,
    Remote,
    Gemini,
    Local_3b,
    Local_7b,
    Custom
};

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

enum class FileScanOptions {
    None        = 0,
    Files       = 1 << 0,   // 0001
    Directories = 1 << 1,   // 0010
    HiddenFiles = 1 << 2    // 0100
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

#endif
