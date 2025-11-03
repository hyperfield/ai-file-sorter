#ifndef MOVABLECATEGORIZEDFILE_HPP
#define MOVABLECATEGORIZEDFILE_HPP

#include <string>
#include <filesystem>

class MovableCategorizedFile {
public:
    MovableCategorizedFile();
    MovableCategorizedFile(const std::string& dir_path,
                    const std::string& cat,
                    const std::string& subcat,
                    const std::string& file_name,
                    const std::string& file_type);
    ~MovableCategorizedFile();
    void create_cat_dirs(bool use_subcategory);
    bool move_file(bool use_subcategory);

    std::string get_subcategory_path() const;
    std::string get_category_path() const;
    std::string get_destination_path() const;
    std::string get_file_name() const;
    std::string get_dir_path() const;
    std::string get_category() const;
    std::string get_subcategory() const;
    std::string get_file_type() const; // TODO
    void set_category(std::string& category);
    void set_subcategory(std::string& subcategory);

private:
    struct MovePaths {
        std::filesystem::path source;
        std::filesystem::path destination;
    };

    MovePaths build_move_paths(bool use_subcategory) const;
    bool source_is_available(const std::filesystem::path& source_path) const;
    bool destination_is_available(const std::filesystem::path& destination_path) const;
    bool perform_move(const std::filesystem::path& source_path,
                      const std::filesystem::path& destination_path) const;

    std::string file_name;
    std::string file_type;
    std::string dir_path;
    std::string category;
    std::string subcategory;
    std::filesystem::path category_path;
    std::filesystem::path subcategory_path;
    std::filesystem::path destination_path;
};

#endif
