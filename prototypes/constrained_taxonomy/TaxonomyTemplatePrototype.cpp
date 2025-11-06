#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../../app/include/Types.hpp"  // Reuse shared FileType definition

struct CategoryOption {
    std::string category;
    std::string subcategory;
};

using TemplateMap = std::map<std::string, std::vector<CategoryOption>>;

// Simple seed data; extend this map while prototyping.
static const TemplateMap kTemplates = {
    {".iso", {
        {"Operating Systems", "Ubuntu distributions"},
        {"Operating Systems", "Windows"},
        {"Operating Systems", "Live media"}
    }},
    {".pdf", {
        {"Documents", "Manuals"},
        {"Documents", "Technical guides"},
        {"Documents", "Financial"}
    }},
    {".png", {
        {"Images", "Digital art"},
        {"Images", "Screenshots"}
    }}
};

std::string to_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string extension_from(std::string_view path)
{
    const auto pos = path.find_last_of('.');
    if (pos == std::string_view::npos) {
        return {};
    }
    return to_lower(std::string(path.substr(pos)));
}

const std::vector<CategoryOption>* lookup_template(std::string_view path)
{
    const std::string ext = extension_from(path);
    auto it = kTemplates.find(ext);
    if (it != kTemplates.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string build_constrained_prompt(std::string_view path,
                                     const std::vector<CategoryOption>& options)
{
    std::ostringstream prompt;
    prompt << "You are a sorting assistant. Only use the provided categories.\n";
    prompt << "File: " << path << "\n\nAllowed categories (category -> subcategory):\n";
    for (size_t i = 0; i < options.size(); ++i) {
        prompt << i + 1 << ". " << options[i].category << " : " << options[i].subcategory << "\n";
    }
    prompt << "\nRespond with exactly '<Category> : <Subcategory>' using one of the entries above.";
    return prompt.str();
}

std::optional<CategoryOption> parse_llm_choice(std::string_view response,
                                               const std::vector<CategoryOption>& options)
{
    for (const auto& option : options) {
        const std::string needle = option.category + " : " + option.subcategory;
        if (response.find(needle) != std::string_view::npos) {
            return option;
        }
    }
    return std::nullopt;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: taxonomy_prototype <file-path> [context]\n";
        return 1;
    }

    const std::string path = argv[1];
    const std::string context = (argc >= 3) ? argv[2] : "";

    const auto* options = lookup_template(path);
    if (!options) {
        std::cout << "No template registered for '" << path << "'.\n";
        return 0;
    }

    const FileType assumed_type = FileType::File;
    (void)assumed_type; // placeholder showing reuse of shared enums

    std::string prompt = build_constrained_prompt(path, *options);
    std::cout << "\n--- Prompt preview ---\n" << prompt << "\n----------------------\n";

    std::cout << "\nSimulate LLM response (type one of the allowed pairs):\n> ";
    std::string response;
    std::getline(std::cin, response);

    auto chosen = parse_llm_choice(response, *options);
    if (!chosen) {
        std::cout << "Response did not match any option.\n";
        return 0;
    }

    std::cout << "Matched category: " << chosen->category
              << " / " << chosen->subcategory << "\n";

    if (!context.empty()) {
        std::cout << "Context hint: " << context << "\n";
    }

    return 0;
}
