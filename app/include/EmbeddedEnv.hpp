#ifndef EMBEDDED_ENV_H
#define EMBEDDED_ENV_H

#include <string>
#include <stdexcept>

class EmbeddedEnv {
public:
    explicit EmbeddedEnv(const std::string& resource_path);
    void load_env();

private:
    std::string resource_path_;
    std::string extract_env_content();
    void parse_env(const std::string& env_content);
    std::string trim(const std::string& str);
};

#endif
