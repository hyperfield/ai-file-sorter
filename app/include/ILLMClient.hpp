#pragma once
#include "Types.hpp"
#include <string>

class ILLMClient {
public:
    virtual ~ILLMClient() = default;
    virtual std::string categorize_file(const std::string& file_name,
                                        const std::string& file_path,
                                        FileType file_type) = 0;
    virtual std::string complete_prompt(const std::string& prompt,
                                        int max_tokens) = 0;
};
