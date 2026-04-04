#pragma once

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
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTextBrowser;

namespace pdftools_gui::pages {

class Pdf2DocxPage : public QWidget {
  Q_OBJECT

 public:
  explicit Pdf2DocxPage(pdftools_gui::services::TaskManager* task_manager,
                        pdftools_gui::services::SettingsService* settings,
                        QWidget* parent = nullptr);

 private:
  void UpdateInputPdfInfo();
  void RefreshIrPreview();
  void SubmitTask();
  void HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info);
  void UpdateDumpIrEnabled(bool enabled);

  pdftools_gui::services::TaskManager* task_manager_ = nullptr;
  pdftools_gui::services::SettingsService* settings_ = nullptr;
  std::unique_ptr<pdftools_gui::services::PreviewService> preview_service_;
  pdftools_gui::widgets::PathPicker* input_picker_ = nullptr;
  QLabel* input_info_label_ = nullptr;
  pdftools_gui::widgets::PathPicker* output_picker_ = nullptr;
  QCheckBox* enable_dump_ir_checkbox_ = nullptr;
  pdftools_gui::widgets::PathPicker* dump_ir_picker_ = nullptr;
  QCheckBox* extract_images_checkbox_ = nullptr;
  QCheckBox* anchored_images_checkbox_ = nullptr;
  QCheckBox* font_fallback_checkbox_ = nullptr;
  QCheckBox* overwrite_checkbox_ = nullptr;
  QSpinBox* preview_page_spin_ = nullptr;
  QPushButton* preview_refresh_button_ = nullptr;
  QLabel* preview_summary_label_ = nullptr;
  QTextBrowser* preview_browser_ = nullptr;
  QListWidget* preview_images_list_ = nullptr;
  QPushButton* run_button_ = nullptr;
  QLabel* task_label_ = nullptr;
  QPlainTextEdit* result_view_ = nullptr;
  qint64 last_task_id_ = 0;
};

}  // namespace pdftools_gui::pages
