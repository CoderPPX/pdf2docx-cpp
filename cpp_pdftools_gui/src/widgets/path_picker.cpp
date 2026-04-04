#include "pdftools_gui/widgets/path_picker.hpp"

#include <QCompleter>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QStringListModel>

namespace pdftools_gui::widgets {

PathPicker::PathPicker(QWidget* parent) : QWidget(parent) {
  line_edit_ = new QLineEdit(this);
  line_edit_->setClearButtonEnabled(true);
  browse_button_ = new QPushButton(QStringLiteral("Browse"), this);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(line_edit_, 1);
  layout->addWidget(browse_button_);

  connect(browse_button_, &QPushButton::clicked, this, &PathPicker::OnBrowseClicked);
  connect(line_edit_, &QLineEdit::textChanged, this, &PathPicker::PathChanged);
}

void PathPicker::SetMode(Mode mode) {
  mode_ = mode;
}

void PathPicker::SetDialogTitle(const QString& title) {
  dialog_title_ = title;
}

void PathPicker::SetNameFilter(const QString& filter) {
  name_filter_ = filter;
}

void PathPicker::SetPath(const QString& path) {
  line_edit_->setText(path);
}

void PathPicker::SetPlaceholderText(const QString& text) {
  line_edit_->setPlaceholderText(text);
}

void PathPicker::SetSuggestions(const QStringList& suggestions) {
  QStringList deduplicated;
  deduplicated.reserve(suggestions.size());
  for (const QString& item : suggestions) {
    const QString trimmed = item.trimmed();
    if (trimmed.isEmpty() || deduplicated.contains(trimmed)) {
      continue;
    }
    deduplicated.push_back(trimmed);
  }
  suggestions_ = deduplicated;

  if (line_edit_->completer() != nullptr) {
    line_edit_->completer()->deleteLater();
  }
  auto* model = new QStringListModel(suggestions_, this);
  auto* completer = new QCompleter(model, this);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setFilterMode(Qt::MatchContains);
  line_edit_->setCompleter(completer);
}

QStringList PathPicker::Suggestions() const {
  return suggestions_;
}

QString PathPicker::Path() const {
  return line_edit_->text().trimmed();
}

void PathPicker::OnBrowseClicked() {
  const QString title = dialog_title_.isEmpty() ? QStringLiteral("Select Path") : dialog_title_;
  QString selected;

  switch (mode_) {
    case Mode::kOpenFile:
      selected = QFileDialog::getOpenFileName(this, title, Path(), name_filter_);
      break;
    case Mode::kSaveFile:
      selected = QFileDialog::getSaveFileName(this, title, Path(), name_filter_);
      break;
    case Mode::kDirectory:
      selected = QFileDialog::getExistingDirectory(this, title, Path());
      break;
  }

  if (!selected.isEmpty()) {
    line_edit_->setText(selected);
  }
}

}  // namespace pdftools_gui::widgets
