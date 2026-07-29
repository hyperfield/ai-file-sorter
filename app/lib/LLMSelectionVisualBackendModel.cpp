#include "LLMSelectionVisualBackendModel.hpp"

#include <algorithm>

namespace LLMSelectionVisualBackendModel {

QString visual_backend_combo_label(const VisualModelDescriptor& backend,
                                   const QString& recommended_label)
{
    QString label = QString::fromUtf8(backend.display_name);
    if (std::string_view(backend.id) == std::string_view(default_visual_model_descriptor().id)) {
        label = QStringLiteral("%1 (%2)").arg(label, recommended_label);
    }
    return label;
}

std::vector<LLMSelectionVisualBackendItem> build_visual_backend_items(
    const std::vector<CustomLLM>& custom_llms,
    const QString& recommended_label,
    const QString& custom_label_template)
{
    std::vector<LLMSelectionVisualBackendItem> items;
    items.reserve(visual_model_descriptors().size() + custom_llms.size());

    for (const auto& backend : visual_model_descriptors()) {
        items.push_back({visual_backend_combo_label(backend, recommended_label),
                         std::string(backend.id)});
    }

    for (const auto& custom : custom_llms) {
        if (!is_visual_custom_llm(custom)) {
            continue;
        }
        items.push_back({custom_label_template.arg(QString::fromStdString(custom.name)),
                         custom_visual_model_id_for_llm(custom.id)});
    }

    return items;
}

int index_of_visual_backend_id(const std::vector<LLMSelectionVisualBackendItem>& items,
                               std::string_view id)
{
    const auto it = std::find_if(items.begin(), items.end(), [id](const auto& item) {
        return std::string_view(item.id) == id;
    });
    if (it == items.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(items.begin(), it));
}

std::string choose_visual_backend_id(std::string_view requested_id,
                                     const std::vector<LLMSelectionVisualBackendItem>& items)
{
    if (index_of_visual_backend_id(items, requested_id) >= 0) {
        return std::string(requested_id);
    }

    const std::string_view default_id = default_visual_model_descriptor().id;
    if (index_of_visual_backend_id(items, default_id) >= 0) {
        return std::string(default_id);
    }

    return items.empty() ? std::string() : items.front().id;
}

const VisualModelDescriptor* selected_visual_model_descriptor(std::string_view selected_id)
{
    if (is_custom_visual_model_id(selected_id)) {
        return &custom_visual_model_descriptor();
    }
    if (const auto* descriptor = find_visual_model_descriptor(selected_id)) {
        return descriptor;
    }
    return &default_visual_model_descriptor();
}

std::string canonical_visual_backend_id(std::string_view selected_id)
{
    if (is_custom_visual_model_id(selected_id)) {
        return std::string(selected_id);
    }
    const auto* descriptor = selected_visual_model_descriptor(selected_id);
    return descriptor ? std::string(descriptor->id) : std::string();
}

} // namespace LLMSelectionVisualBackendModel
