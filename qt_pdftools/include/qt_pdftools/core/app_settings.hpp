#pragma once

#include <QString>

class QSettings;

namespace qt_pdftools::core {

struct AppSettings {
  QString theme_mode = QStringLiteral("system");
  QString last_open_dir;
};

AppSettings LoadAppSettings(QSettings* settings);
void SaveAppSettings(QSettings* settings, const AppSettings& values);

}  // namespace qt_pdftools::core
