#include "pdftools_gui/widgets/file_list_editor.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace pdftools_gui::widgets {

FileListEditor::FileListEditor(QWidget* parent) : QWidget(parent) {
  list_widget_ = new QListWidget(this);
  list_widget_->setSelectionMode(QAbstractItemView::SingleSelection);
  list_widget_->setDragDropMode(QAbstractItemView::InternalMove);
  list_widget_->setDefaultDropAction(Qt::MoveAction);

  add_button_ = new QPushButton(QStringLiteral("Add"), this);
  remove_button_ = new QPushButton(QStringLiteral("Remove"), this);
  up_button_ = new QPushButton(QStringLiteral("Up"), this);
  down_button_ = new QPushButton(QStringLiteral("Down"), this);
  clear_button_ = new QPushButton(QStringLiteral("Clear"), this);

  auto* buttons_layout = new QVBoxLayout();
  buttons_layout->addWidget(add_button_);
  buttons_layout->addWidget(remove_button_);
  buttons_layout->addWidget(up_button_);
  buttons_layout->addWidget(down_button_);
  buttons_layout->addWidget(clear_button_);
  buttons_layout->addStretch(1);

  auto* root_layout = new QHBoxLayout(this);
  root_layout->addWidget(list_widget_, 1);
  root_layout->addLayout(buttons_layout);

  connect(add_button_, &QPushButton::clicked, this, &FileListEditor::AddFiles);
  connect(remove_button_, &QPushButton::clicked, this, &FileListEditor::RemoveSelected);
  connect(up_button_, &QPushButton::clicked, this, &FileListEditor::MoveSelectedUp);
  connect(down_button_, &QPushButton::clicked, this, &FileListEditor::MoveSelectedDown);
  connect(clear_button_, &QPushButton::clicked, list_widget_, &QListWidget::clear);
  connect(clear_button_, &QPushButton::clicked, this, &FileListEditor::EmitPathsChanged);
  connect(list_widget_->model(), &QAbstractItemModel::rowsMoved, this, &FileListEditor::EmitPathsChanged);
}

QStringList FileListEditor::Paths() const {
  QStringList values;
  values.reserve(list_widget_->count());
  for (int i = 0; i < list_widget_->count(); ++i) {
    values.push_back(list_widget_->item(i)->text());
  }
  return values;
}

void FileListEditor::SetPaths(const QStringList& paths) {
  list_widget_->clear();
  list_widget_->addItems(paths);
  EmitPathsChanged();
}

void FileListEditor::SetDialogTitle(const QString& title) {
  dialog_title_ = title;
}

void FileListEditor::SetNameFilter(const QString& filter) {
  name_filter_ = filter;
}

void FileListEditor::AddFiles() {
  const QString title = dialog_title_.isEmpty() ? QStringLiteral("Select Files") : dialog_title_;
  const QStringList selected = QFileDialog::getOpenFileNames(this, title, QString(), name_filter_);
  if (selected.isEmpty()) {
    return;
  }

  for (const QString& file : selected) {
    if (file.isEmpty()) {
      continue;
    }
    if (list_widget_->findItems(file, Qt::MatchExactly).isEmpty()) {
      list_widget_->addItem(file);
    }
  }

  EmitPathsChanged();
}

void FileListEditor::RemoveSelected() {
  const int row = list_widget_->currentRow();
  if (row < 0) {
    return;
  }
  delete list_widget_->takeItem(row);
  EmitPathsChanged();
}

void FileListEditor::MoveSelectedUp() {
  const int row = list_widget_->currentRow();
  if (row <= 0) {
    return;
  }

  QListWidgetItem* item = list_widget_->takeItem(row);
  list_widget_->insertItem(row - 1, item);
  list_widget_->setCurrentRow(row - 1);
  EmitPathsChanged();
}

void FileListEditor::MoveSelectedDown() {
  const int row = list_widget_->currentRow();
  if (row < 0 || row >= list_widget_->count() - 1) {
    return;
  }

  QListWidgetItem* item = list_widget_->takeItem(row);
  list_widget_->insertItem(row + 1, item);
  list_widget_->setCurrentRow(row + 1);
  EmitPathsChanged();
}

void FileListEditor::EmitPathsChanged() {
  emit PathsChanged(Paths());
}

}  // namespace pdftools_gui::widgets
