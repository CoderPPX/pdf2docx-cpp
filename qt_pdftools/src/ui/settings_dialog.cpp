#include "qt_pdftools/ui/settings_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace qt_pdftools::ui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Settings"));
  resize(520, 260);

  auto* root = new QVBoxLayout(this);
  auto* form = new QFormLayout();

  theme_mode_combo_ = new QComboBox(this);
  theme_mode_combo_->addItem(QStringLiteral("System"), QStringLiteral("system"));
  theme_mode_combo_->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
  theme_mode_combo_->addItem(QStringLiteral("Light"), QStringLiteral("light"));

  form->addRow(QStringLiteral("Theme"), theme_mode_combo_);

  auto* hint = new QLabel(
      QStringLiteral("说明：当前版本仅启用 native-lib（in-process）后端。"),
      this);
  hint->setWordWrap(true);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

  root->addLayout(form);
  root->addWidget(hint);
  root->addStretch(1);
  root->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::SetValues(const qt_pdftools::core::AppSettings& values) {
  int theme_index = theme_mode_combo_->findData(values.theme_mode);
  if (theme_index < 0) {
    theme_index = 0;
  }
  theme_mode_combo_->setCurrentIndex(theme_index);

}

qt_pdftools::core::AppSettings SettingsDialog::Values() const {
  qt_pdftools::core::AppSettings values;
  values.theme_mode = theme_mode_combo_->currentData().toString();
  return values;
}

}  // namespace qt_pdftools::ui
