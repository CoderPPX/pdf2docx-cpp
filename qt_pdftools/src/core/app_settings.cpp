#include "qt_pdftools/core/app_settings.hpp"

#include <QDir>
#include <QSettings>

namespace qt_pdftools::core {

AppSettings LoadAppSettings(QSettings* settings) {
  AppSettings values;
  if (settings == nullptr) {
    return values;
  }

  values.theme_mode = settings->value(QStringLiteral("ui/theme_mode"), values.theme_mode).toString();
  values.last_open_dir =
      settings->value(QStringLiteral("ui/last_open_dir"), QDir::homePath()).toString();
  return values;
}

void SaveAppSettings(QSettings* settings, const AppSettings& values) {
  if (settings == nullptr) {
    return;
  }
  settings->setValue(QStringLiteral("ui/theme_mode"), values.theme_mode);
  settings->setValue(QStringLiteral("ui/last_open_dir"), values.last_open_dir);
}

}  // namespace qt_pdftools::core
