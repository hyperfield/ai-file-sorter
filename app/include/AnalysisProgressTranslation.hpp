#pragma once
#include <QString>

/** @brief UI-neutral lookup for coordinator messages and shared MainApp labels. */
namespace AnalysisProgressTranslation {
/** @brief Translate through the extraction context, then the shared-label context. @param text Source message. */
QString translate(const char* text);
}
