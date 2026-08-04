#ifndef LLM_SELECTION_VISUAL_BACKEND_MODEL_HPP
#define LLM_SELECTION_VISUAL_BACKEND_MODEL_HPP

#include "Types.hpp"
#include "VisualModelCatalog.hpp"

#include <QString>

#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Display item for the LLM selection dialog's visual backend combo box.
 */
struct LLMSelectionVisualBackendItem {
    QString label;
    std::string id;
};

namespace LLMSelectionVisualBackendModel {

/**
 * @brief Build a display label for a built-in visual backend.
 * @param backend Built-in visual model descriptor.
 * @param recommended_label Localized text for the recommended marker.
 * @return Combo-box display label.
 */
QString visual_backend_combo_label(const VisualModelDescriptor& backend,
                                   const QString& recommended_label);

/**
 * @brief Build built-in and custom visual backend combo items.
 * @param custom_llms Configured custom local LLMs.
 * @param recommended_label Localized text for the recommended marker.
 * @param custom_label_template Localized template containing `%1` for the custom LLM name.
 * @return Ordered combo items.
 */
std::vector<LLMSelectionVisualBackendItem> build_visual_backend_items(
    const std::vector<CustomLLM>& custom_llms,
    const QString& recommended_label,
    const QString& custom_label_template);

/**
 * @brief Pick the best selectable visual backend id.
 * @param requested_id Previously selected or requested backend id.
 * @param items Available combo items.
 * @return Requested id when present, otherwise the default id, otherwise the first item id.
 */
std::string choose_visual_backend_id(std::string_view requested_id,
                                     const std::vector<LLMSelectionVisualBackendItem>& items);

/**
 * @brief Find an item index by backend id.
 * @param items Available combo items.
 * @param id Backend id to find.
 * @return Zero-based index, or -1 when absent.
 */
int index_of_visual_backend_id(const std::vector<LLMSelectionVisualBackendItem>& items,
                               std::string_view id);

/**
 * @brief Return the effective descriptor for a selected visual backend id.
 * @param selected_id Selected visual backend id.
 * @return Custom descriptor for custom ids, matching built-in descriptor, or default descriptor.
 */
const VisualModelDescriptor* selected_visual_model_descriptor(std::string_view selected_id);

/**
 * @brief Return the canonical selected id for a visual backend selection.
 * @param selected_id Selected visual backend id.
 * @return Custom id unchanged, matching built-in id, or default built-in id.
 */
std::string canonical_visual_backend_id(std::string_view selected_id);

} // namespace LLMSelectionVisualBackendModel

#endif // LLM_SELECTION_VISUAL_BACKEND_MODEL_HPP
