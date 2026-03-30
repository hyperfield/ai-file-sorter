#pragma once

#include <string>
#include <vector>

struct CloudPathMatch {
    bool matched{false};
    int confidence{0};
};

CloudPathMatch detect_cloud_path_match(const std::string& root_path,
                                       const std::vector<std::string>& path_markers,
                                       const std::vector<std::string>& env_var_names);
