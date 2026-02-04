#pragma once

#include <QString>

#ifndef AI_FILE_SORTER_APP_NAME
#define AI_FILE_SORTER_APP_NAME "AI File Sorter"
#endif

inline QString app_display_name() {
    return QStringLiteral(AI_FILE_SORTER_APP_NAME);
}
