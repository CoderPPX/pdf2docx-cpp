#pragma once

#include <QDialog>

#include "qt_pdftools/core/app_settings.hpp"

class QComboBox;

namespace qt_pdftools::ui {

class SettingsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(QWidget* parent = nullptr);

  void SetValues(const qt_pdftools::core::AppSettings& values);
  qt_pdftools::core::AppSettings Values() const;

 private:
  QComboBox* theme_mode_combo_ = nullptr;
};

}  // namespace qt_pdftools::ui
