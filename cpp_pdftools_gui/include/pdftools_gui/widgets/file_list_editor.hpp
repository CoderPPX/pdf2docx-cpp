#pragma once

#include <QWidget>

class QListWidget;
class QPushButton;

namespace pdftools_gui::widgets {

class FileListEditor : public QWidget {
  Q_OBJECT

 public:
  explicit FileListEditor(QWidget* parent = nullptr);

  QStringList Paths() const;
  void SetPaths(const QStringList& paths);
  void SetDialogTitle(const QString& title);
  void SetNameFilter(const QString& filter);

 signals:
  void PathsChanged(const QStringList& paths);

 private:
  void AddFiles();
  void RemoveSelected();
  void MoveSelectedUp();
  void MoveSelectedDown();
  void EmitPathsChanged();

  QListWidget* list_widget_ = nullptr;
  QPushButton* add_button_ = nullptr;
  QPushButton* remove_button_ = nullptr;
  QPushButton* up_button_ = nullptr;
  QPushButton* down_button_ = nullptr;
  QPushButton* clear_button_ = nullptr;
  QString dialog_title_;
  QString name_filter_;
};

}  // namespace pdftools_gui::widgets
