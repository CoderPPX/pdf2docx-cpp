#pragma once

#include <memory>

#include <QStringList>
#include <QWidget>

#include "pdftools_gui/services/preview_service.hpp"

namespace pdftools_gui::services {
class SettingsService;
class TaskManager;
struct TaskInfo;
}  // namespace pdftools_gui::services

namespace pdftools_gui::widgets {
class FileListEditor;
class PathPicker;
}

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QSpinBox;
class QTextBrowser;

namespace pdftools_gui::pages {

class MergePage : public QWidget {
  Q_OBJECT

 public:
  explicit MergePage(pdftools_gui::services::TaskManager* task_manager,
                     pdftools_gui::services::SettingsService* settings,
                     QWidget* parent = nullptr);

 private:
  void RefreshInputSummary(const QStringList& paths);
  void RefreshPreviewSourceList(const QStringList& paths);
  void RefreshCurrentPdfPreview();
  void SubmitTask();
  void HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info);

  pdftools_gui::services::TaskManager* task_manager_ = nullptr;
  pdftools_gui::services::SettingsService* settings_ = nullptr;
  pdftools_gui::widgets::FileListEditor* input_files_ = nullptr;
  QLabel* input_summary_label_ = nullptr;
  std::unique_ptr<pdftools_gui::services::PreviewService> preview_service_;
  QComboBox* preview_pdf_combo_ = nullptr;
  QSpinBox* preview_page_spin_ = nullptr;
  QPushButton* preview_refresh_button_ = nullptr;
  QLabel* preview_summary_label_ = nullptr;
  QTextBrowser* preview_browser_ = nullptr;
  pdftools_gui::widgets::PathPicker* output_picker_ = nullptr;
  QCheckBox* overwrite_checkbox_ = nullptr;
  QPushButton* run_button_ = nullptr;
  QLabel* task_label_ = nullptr;
  QPlainTextEdit* result_view_ = nullptr;
  qint64 last_task_id_ = 0;
};

}  // namespace pdftools_gui::pages
