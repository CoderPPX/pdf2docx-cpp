#pragma once

#include <cstdint>
#include <memory>

#include <QWidget>

#include "pdftools_gui/services/preview_service.hpp"

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
class QSpinBox;
class QTabWidget;
class QTextBrowser;

namespace pdftools_gui::pages {

class PageEditPage : public QWidget {
  Q_OBJECT

 public:
  explicit PageEditPage(pdftools_gui::services::TaskManager* task_manager,
                        pdftools_gui::services::SettingsService* settings,
                        QWidget* parent = nullptr);

 private:
  void RefreshPdfInfo();
  uint32_t RefreshPdfInfoForLabel(const QString& path, QLabel* label);
  void RefreshSelectedPagePreview();
  void SubmitTask();
  void HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info);

  pdftools_gui::services::TaskManager* task_manager_ = nullptr;
  pdftools_gui::services::SettingsService* settings_ = nullptr;
  std::unique_ptr<pdftools_gui::services::PreviewService> preview_service_;
  pdftools_gui::widgets::PathPicker* input_picker_ = nullptr;
  QLabel* input_info_label_ = nullptr;
  pdftools_gui::widgets::PathPicker* output_picker_ = nullptr;
  QCheckBox* overwrite_checkbox_ = nullptr;
  QTabWidget* tab_widget_ = nullptr;

  QSpinBox* delete_page_spin_ = nullptr;

  QSpinBox* insert_at_spin_ = nullptr;
  pdftools_gui::widgets::PathPicker* insert_source_picker_ = nullptr;
  QLabel* insert_source_info_label_ = nullptr;
  QSpinBox* insert_source_page_spin_ = nullptr;

  QSpinBox* replace_page_spin_ = nullptr;
  pdftools_gui::widgets::PathPicker* replace_source_picker_ = nullptr;
  QLabel* replace_source_info_label_ = nullptr;
  QSpinBox* replace_source_page_spin_ = nullptr;

  QPushButton* run_button_ = nullptr;
  QPushButton* preview_refresh_button_ = nullptr;
  QLabel* preview_summary_label_ = nullptr;
  QTextBrowser* preview_browser_ = nullptr;
  QLabel* task_label_ = nullptr;
  QPlainTextEdit* result_view_ = nullptr;
  qint64 last_task_id_ = 0;
};

}  // namespace pdftools_gui::pages
