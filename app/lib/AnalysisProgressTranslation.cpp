#include "AnalysisProgressTranslation.hpp"
#include <QCoreApplication>

QString AnalysisProgressTranslation::translate(const char* text)
{
    const QString translated = QCoreApplication::translate("AnalysisCoordinator", text);
    if (translated != QString::fromUtf8(text)) return translated;
    return QCoreApplication::translate("MainApp", text);
}
