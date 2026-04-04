#pragma once

#include <QWidget>

namespace pdftools_gui::services {
class SettingsService;
class TaskManager;
struct TaskInfo;
}  // namespace pdftools_gui::services

namespace pdftools_gui::widgets {
class PathPicker;
}

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace pdftools_gui::pages {

class AttachmentsPage : public QWidget {
  Q_OBJECT

 public:
  explicit AttachmentsPage(pdftools_gui::services::TaskManager* task_manager,
                           pdftools_gui::services::SettingsService* settings,
                           QWidget* parent = nullptr);

 private:
  void SubmitTask();
  void HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info);

  pdftools_gui::services::TaskManager* task_manager_ = nullptr;
  pdftools_gui::services::SettingsService* settings_ = nullptr;
  pdftools_gui::widgets::PathPicker* input_picker_ = nullptr;
  pdftools_gui::widgets::PathPicker* output_dir_picker_ = nullptr;
  QCheckBox* strict_checkbox_ = nullptr;
  QCheckBox* overwrite_checkbox_ = nullptr;
  QPushButton* run_button_ = nullptr;
  QLabel* task_label_ = nullptr;
  QPlainTextEdit* result_view_ = nullptr;
  qint64 last_task_id_ = 0;
};

}  // namespace pdftools_gui::pages
