#pragma once

#include <QString>
#include <QWidget>

namespace pdftools_gui::services {
class SettingsService;
class TaskManager;
}  // namespace pdftools_gui::services

class QLabel;
class QPushButton;
class QSpinBox;
namespace pdftools_gui::widgets {
class PathPicker;
}

namespace pdftools_gui::pages {

class SettingsPage : public QWidget {
  Q_OBJECT

 public:
  explicit SettingsPage(pdftools_gui::services::TaskManager* task_manager,
                        pdftools_gui::services::SettingsService* settings,
                        QWidget* parent = nullptr);

 signals:
  void TaskHistoryCleared();
  void ReportDirectoryChanged(const QString& path);

 private:
  void ApplyTaskRetention();
  void ClearRecentHistory();
  void ClearTaskHistory();
  void ResetWindowState();

  pdftools_gui::services::TaskManager* task_manager_ = nullptr;
  pdftools_gui::services::SettingsService* settings_ = nullptr;

  QSpinBox* max_completed_tasks_spin_ = nullptr;
  QSpinBox* max_concurrent_tasks_spin_ = nullptr;
  pdftools_gui::widgets::PathPicker* report_directory_picker_ = nullptr;
  QPushButton* apply_button_ = nullptr;
  QPushButton* clear_recent_button_ = nullptr;
  QPushButton* clear_task_history_button_ = nullptr;
  QPushButton* reset_window_state_button_ = nullptr;
  QLabel* status_label_ = nullptr;
};

}  // namespace pdftools_gui::pages
