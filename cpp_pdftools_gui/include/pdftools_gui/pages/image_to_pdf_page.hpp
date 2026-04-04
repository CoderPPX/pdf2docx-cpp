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
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace pdftools_gui::pages {

class ImageToPdfPage : public QWidget {
  Q_OBJECT

 public:
  explicit ImageToPdfPage(pdftools_gui::services::TaskManager* task_manager,
                          pdftools_gui::services::SettingsService* settings,
                          QWidget* parent = nullptr);

 private:
  void RefreshImagePreview(const QStringList& paths);
  void SubmitTask();
  void HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info);
  void UpdatePageOptionsEnabled(bool use_image_size);

  pdftools_gui::services::TaskManager* task_manager_ = nullptr;
  pdftools_gui::services::SettingsService* settings_ = nullptr;
  std::unique_ptr<pdftools_gui::services::PreviewService> preview_service_;
  pdftools_gui::widgets::FileListEditor* image_files_ = nullptr;
  QLabel* image_preview_summary_label_ = nullptr;
  QListWidget* image_preview_list_ = nullptr;
  pdftools_gui::widgets::PathPicker* output_picker_ = nullptr;
  QCheckBox* use_image_size_checkbox_ = nullptr;
  QDoubleSpinBox* page_width_spin_ = nullptr;
  QDoubleSpinBox* page_height_spin_ = nullptr;
  QDoubleSpinBox* margin_spin_ = nullptr;
  QComboBox* scale_combo_ = nullptr;
  QCheckBox* overwrite_checkbox_ = nullptr;
  QPushButton* run_button_ = nullptr;
  QLabel* task_label_ = nullptr;
  QPlainTextEdit* result_view_ = nullptr;
  qint64 last_task_id_ = 0;
};

}  // namespace pdftools_gui::pages
