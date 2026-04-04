#include "pdftools_gui/app/main_window.hpp"

#include <QCloseEvent>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QWidget>

#include "pdftools_gui/models/task_item_model.hpp"
#include "pdftools_gui/pages/attachments_page.hpp"
#include "pdftools_gui/pages/image_to_pdf_page.hpp"
#include "pdftools_gui/pages/merge_page.hpp"
#include "pdftools_gui/pages/page_edit_page.hpp"
#include "pdftools_gui/pages/pdf2docx_page.hpp"
#include "pdftools_gui/pages/settings_page.hpp"
#include "pdftools_gui/pages/text_extract_page.hpp"
#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/task_center_widget.hpp"

namespace pdftools_gui::app {

namespace {

bool IsFinalState(pdftools_gui::services::TaskState state) {
  return state == pdftools_gui::services::TaskState::kSucceeded ||
         state == pdftools_gui::services::TaskState::kFailed ||
         state == pdftools_gui::services::TaskState::kCanceled;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  settings_ = std::make_unique<pdftools_gui::services::SettingsService>();
  service_ = std::make_shared<pdftools_gui::services::PdfToolsService>();
  const int max_completed_tasks =
      settings_->ReadValue(QStringLiteral("settings/max_completed_tasks"), 200).toInt();
  const int max_concurrent_tasks =
      settings_->ReadValue(QStringLiteral("settings/max_concurrent_tasks"), 2).toInt();
  task_manager_ = std::make_unique<pdftools_gui::services::TaskManager>(
      service_, nullptr, max_completed_tasks, max_concurrent_tasks);
  task_model_ = std::make_unique<pdftools_gui::models::TaskItemModel>();

  BuildUi();
  RestoreWindowState();
  RestoreTaskHistory();

  connect(task_manager_.get(),
          &pdftools_gui::services::TaskManager::TaskChanged,
          task_model_.get(),
          &pdftools_gui::models::TaskItemModel::UpsertTask);

  connect(task_manager_.get(), &pdftools_gui::services::TaskManager::TaskCompleted, this,
          [this](const pdftools_gui::services::TaskInfo& info) {
            statusBar()->showMessage(
                QStringLiteral("任务 #%1 完成: %2").arg(info.id).arg(info.summary),
                8000);
          });

  connect(settings_page_, &pdftools_gui::pages::SettingsPage::TaskHistoryCleared, this, [this]() {
    if (task_model_ != nullptr) {
      task_model_->RemoveFinalTasks();
    }
    statusBar()->showMessage(QStringLiteral("任务历史已清空"), 5000);
  });

  connect(settings_page_, &pdftools_gui::pages::SettingsPage::ReportDirectoryChanged, this, [this](const QString& path) {
    if (task_center_widget_ != nullptr) {
      task_center_widget_->SetReportDirectory(path);
    }
    statusBar()->showMessage(QStringLiteral("任务报告目录已更新"), 5000);
  });
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
  SaveTaskHistory();
  SaveWindowState();
  QMainWindow::closeEvent(event);
}

void MainWindow::BuildUi() {
  setWindowTitle(QStringLiteral("PDFTools GUI"));
  resize(1320, 860);

  auto* central = new QWidget(this);
  auto* root_layout = new QHBoxLayout(central);

  navigation_ = new QListWidget(central);
  navigation_->addItem(QStringLiteral("PDF 合并"));
  navigation_->addItem(QStringLiteral("文本提取"));
  navigation_->addItem(QStringLiteral("附件提取"));
  navigation_->addItem(QStringLiteral("图片转 PDF"));
  navigation_->addItem(QStringLiteral("页面编辑"));
  navigation_->addItem(QStringLiteral("PDF 转 DOCX"));
  navigation_->addItem(QStringLiteral("设置"));
  navigation_->setFixedWidth(180);

  stacked_pages_ = new QStackedWidget(central);

  merge_page_ = new pdftools_gui::pages::MergePage(task_manager_.get(), settings_.get(), stacked_pages_);
  text_extract_page_ =
      new pdftools_gui::pages::TextExtractPage(task_manager_.get(), settings_.get(), stacked_pages_);
  attachments_page_ =
      new pdftools_gui::pages::AttachmentsPage(task_manager_.get(), settings_.get(), stacked_pages_);
  image_to_pdf_page_ =
      new pdftools_gui::pages::ImageToPdfPage(task_manager_.get(), settings_.get(), stacked_pages_);
  page_edit_page_ = new pdftools_gui::pages::PageEditPage(task_manager_.get(), settings_.get(), stacked_pages_);
  pdf2docx_page_ = new pdftools_gui::pages::Pdf2DocxPage(task_manager_.get(), settings_.get(), stacked_pages_);
  settings_page_ = new pdftools_gui::pages::SettingsPage(task_manager_.get(), settings_.get(), stacked_pages_);

  stacked_pages_->addWidget(merge_page_);
  stacked_pages_->addWidget(text_extract_page_);
  stacked_pages_->addWidget(attachments_page_);
  stacked_pages_->addWidget(image_to_pdf_page_);
  stacked_pages_->addWidget(page_edit_page_);
  stacked_pages_->addWidget(pdf2docx_page_);
  stacked_pages_->addWidget(settings_page_);

  root_layout->addWidget(navigation_);
  root_layout->addWidget(stacked_pages_, 1);
  setCentralWidget(central);

  connect(navigation_, &QListWidget::currentRowChanged, stacked_pages_, &QStackedWidget::setCurrentIndex);
  navigation_->setCurrentRow(0);

  auto* task_dock = new QDockWidget(QStringLiteral("任务中心"), this);
  task_center_widget_ = new pdftools_gui::widgets::TaskCenterWidget(task_dock);
  task_center_widget_->SetTaskManager(task_manager_.get());
  task_center_widget_->SetReportDirectory(
      settings_->ReadValue(QStringLiteral("settings/report_directory"), QString()).toString());
  task_center_widget_->SetSourceModel(task_model_.get());
  task_dock->setWidget(task_center_widget_);
  task_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
  addDockWidget(Qt::BottomDockWidgetArea, task_dock);
  task_dock->setMinimumHeight(220);

  statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::RestoreWindowState() {
  const QByteArray geometry = settings_->ReadValue(QStringLiteral("ui/main_window_geometry"), QByteArray()).toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  }

  const QByteArray state = settings_->ReadValue(QStringLiteral("ui/main_window_state"), QByteArray()).toByteArray();
  if (!state.isEmpty()) {
    restoreState(state);
  }
}

void MainWindow::SaveWindowState() {
  settings_->WriteValue(QStringLiteral("ui/main_window_geometry"), saveGeometry());
  settings_->WriteValue(QStringLiteral("ui/main_window_state"), saveState());
}

void MainWindow::RestoreTaskHistory() {
  if (settings_ == nullptr || task_model_ == nullptr || task_manager_ == nullptr) {
    return;
  }

  const QVector<pdftools_gui::services::TaskInfo> tasks =
      settings_->ReadTaskHistory(task_manager_->MaxCompletedTasks());
  if (!tasks.isEmpty()) {
    task_model_->ReplaceAllTasks(tasks);
  }
}

void MainWindow::SaveTaskHistory() {
  if (settings_ == nullptr || task_model_ == nullptr || task_manager_ == nullptr) {
    return;
  }

  const QVector<pdftools_gui::services::TaskInfo> tasks = task_model_->SnapshotTasks();
  QVector<pdftools_gui::services::TaskInfo> completed_tasks;
  completed_tasks.reserve(tasks.size());
  for (const auto& task : tasks) {
    if (IsFinalState(task.state)) {
      completed_tasks.push_back(task);
    }
  }

  settings_->WriteTaskHistory(completed_tasks, task_manager_->MaxCompletedTasks());
}

}  // namespace pdftools_gui::app
