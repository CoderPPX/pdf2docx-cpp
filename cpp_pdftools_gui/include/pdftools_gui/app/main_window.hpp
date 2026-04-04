#pragma once

#include <memory>

#include <QMainWindow>

namespace pdftools_gui::models {
class TaskItemModel;
}

namespace pdftools_gui::pages {
class AttachmentsPage;
class ImageToPdfPage;
class MergePage;
class PageEditPage;
class Pdf2DocxPage;
class SettingsPage;
class TextExtractPage;
}  // namespace pdftools_gui::pages

namespace pdftools_gui::services {
class PdfToolsService;
class SettingsService;
class TaskManager;
struct TaskInfo;
}  // namespace pdftools_gui::services

namespace pdftools_gui::widgets {
class TaskCenterWidget;
}

class QListWidget;
class QStackedWidget;

namespace pdftools_gui::app {

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void BuildUi();
  void RestoreWindowState();
  void SaveWindowState();
  void RestoreTaskHistory();
  void SaveTaskHistory();

  std::shared_ptr<pdftools_gui::services::PdfToolsService> service_;
  std::unique_ptr<pdftools_gui::services::TaskManager> task_manager_;
  std::unique_ptr<pdftools_gui::services::SettingsService> settings_;
  std::unique_ptr<pdftools_gui::models::TaskItemModel> task_model_;

  QListWidget* navigation_ = nullptr;
  QStackedWidget* stacked_pages_ = nullptr;
  pdftools_gui::widgets::TaskCenterWidget* task_center_widget_ = nullptr;

  pdftools_gui::pages::MergePage* merge_page_ = nullptr;
  pdftools_gui::pages::TextExtractPage* text_extract_page_ = nullptr;
  pdftools_gui::pages::AttachmentsPage* attachments_page_ = nullptr;
  pdftools_gui::pages::ImageToPdfPage* image_to_pdf_page_ = nullptr;
  pdftools_gui::pages::PageEditPage* page_edit_page_ = nullptr;
  pdftools_gui::pages::Pdf2DocxPage* pdf2docx_page_ = nullptr;
  pdftools_gui::pages::SettingsPage* settings_page_ = nullptr;
};

}  // namespace pdftools_gui::app
