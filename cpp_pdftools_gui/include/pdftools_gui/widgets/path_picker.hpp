#pragma once

#include <QStringList>
#include <QWidget>

class QLineEdit;
class QPushButton;

namespace pdftools_gui::widgets {

class PathPicker : public QWidget {
  Q_OBJECT

 public:
  enum class Mode {
    kOpenFile = 0,
    kSaveFile,
    kDirectory,
  };

  explicit PathPicker(QWidget* parent = nullptr);

  void SetMode(Mode mode);
  void SetDialogTitle(const QString& title);
  void SetNameFilter(const QString& filter);
  void SetPath(const QString& path);
  void SetPlaceholderText(const QString& text);
  void SetSuggestions(const QStringList& suggestions);
  QStringList Suggestions() const;

  QString Path() const;

 signals:
  void PathChanged(const QString& path);

 private:
  void OnBrowseClicked();

  Mode mode_ = Mode::kOpenFile;
  QString dialog_title_;
  QString name_filter_;
  QStringList suggestions_;
  QLineEdit* line_edit_ = nullptr;
  QPushButton* browse_button_ = nullptr;
};

}  // namespace pdftools_gui::widgets
