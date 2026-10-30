#include "NativeChatTemplate.hpp"

#include "chat.h"
#include "llama.h"

#include <stdexcept>
#include <utility>

namespace {
std::string render_messages(const common_chat_templates* templates,
                            std::string_view system_prompt,
                            std::string_view user_prompt)
{
    common_chat_templates_inputs inputs;
    inputs.enable_thinking = false;
    inputs.add_generation_prompt = true;
    if (!system_prompt.empty()) {
        common_chat_msg system;
        system.role = "system";
        system.content = system_prompt;
        inputs.messages.push_back(std::move(system));
    }
    common_chat_msg user;
    user.role = "user";
    user.content = user_prompt;
    inputs.messages.push_back(std::move(user));
    return common_chat_templates_apply(templates, inputs).prompt.str();
}
} // namespace

std::string NativeChatTemplate::format(const llama_model* model,
                                       std::string_view system_prompt,
                                       std::string_view user_prompt)
{
    if (!model) {
        throw std::invalid_argument("Cannot format a chat prompt without a loaded model");
    }
    const char* source = llama_model_chat_template(model, nullptr);
    if (!source || source[0] == '\0') {
        return system_prompt.empty() ? std::string(user_prompt)
                                    : std::string(system_prompt) + "\n\n" + std::string(user_prompt);
    }
    const auto templates = common_chat_templates_init(model, "");
    return render_messages(templates.get(), system_prompt, user_prompt);
}

std::string NativeChatTemplate::render(std::string_view source,
                                       std::string_view system_prompt,
                                       std::string_view user_prompt)
{
    if (source.empty()) {
        throw std::invalid_argument("Cannot render an empty chat template");
    }
    const auto templates = common_chat_templates_init(nullptr, std::string(source));
    return render_messages(templates.get(), system_prompt, user_prompt);
}
