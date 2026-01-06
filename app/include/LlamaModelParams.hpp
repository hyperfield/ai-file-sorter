#pragma once

#include "llama.h"
#include <memory>
#include <string>

namespace spdlog { class logger; }

llama_model_params build_model_params_for_path(const std::string& model_path,
                                               const std::shared_ptr<spdlog::logger>& logger);
