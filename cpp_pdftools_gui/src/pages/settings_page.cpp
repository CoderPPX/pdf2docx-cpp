#include "pdftools_gui/pages/settings_page.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace pdftools_gui::pages {

SettingsPage::SettingsPage(pdftools_gui::services::TaskManager* task_manager,
                           pdftools_gui::services::SettingsService* settings,
                           QWidget* parent)
    : QWidget(parent), task_manager_(task_manager), settings_(settings) {
  auto* root_layout = new QVBoxLayout(this);

  auto* task_group = new QGroupBox(QStringLiteral("任务设置"), this);
  auto* task_layout = new QFormLayout(task_group);

  max_completed_tasks_spin_ = new QSpinBox(task_group);
  max_completed_tasks_spin_->setObjectName(QStringLiteral("max_completed_tasks_spin"));
  max_completed_tasks_spin_->setRange(10, 5000);
  max_completed_tasks_spin_->setValue(task_manager_ != nullptr ? task_manager_->MaxCompletedTasks() : 200);

  max_concurrent_tasks_spin_ = new QSpinBox(task_group);
  max_concurrent_tasks_spin_->setObjectName(QStringLiteral("max_concurrent_tasks_spin"));
  max_concurrent_tasks_spin_->setRange(1, 64);
  max_concurrent_tasks_spin_->setValue(task_manager_ != nullptr ? task_manager_->MaxConcurrentTasks() : 2);

  report_directory_picker_ = new pdftools_gui::widgets::PathPicker(task_group);
  report_directory_picker_->setObjectName(QStringLiteral("report_directory_picker"));
  report_directory_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kDirectory);
  report_directory_picker_->SetDialogTitle(QStringLiteral("选择报告输出目录"));
  report_directory_picker_->SetPlaceholderText(QStringLiteral("默认使用应用数据目录/task_reports"));
  if (settings_ != nullptr) {
    report_directory_picker_->SetPath(
        settings_->ReadValue(QStringLiteral("settings/report_directory"), QString()).toString());
  }

  apply_button_ = new QPushButton(QStringLiteral("应用任务设置"), task_group);
  apply_button_->setObjectName(QStringLiteral("apply_task_retention_button"));

  task_layout->addRow(QStringLiteral("最大保留完成任务数"), max_completed_tasks_spin_);
  task_layout->addRow(QStringLiteral("最大并发任务数"), max_concurrent_tasks_spin_);
  task_layout->addRow(QStringLiteral("任务报告目录"), report_directory_picker_);
  task_layout->addRow(QString(), apply_button_);

  auto* storage_group = new QGroupBox(QStringLiteral("历史与状态"), this);
  auto* storage_layout = new QVBoxLayout(storage_group);

  clear_recent_button_ = new QPushButton(QStringLiteral("清空最近参数/文件历史"), storage_group);
  clear_recent_button_->setObjectName(QStringLiteral("clear_recent_history_button"));
  clear_task_history_button_ = new QPushButton(QStringLiteral("清空任务历史记录"), storage_group);
  clear_task_history_button_->setObjectName(QStringLiteral("clear_task_history_button"));
  reset_window_state_button_ = new QPushButton(QStringLiteral("重置窗口布局状态"), storage_group);
  reset_window_state_button_->setObjectName(QStringLiteral("reset_window_state_button"));

  storage_layout->addWidget(clear_recent_button_);
  storage_layout->addWidget(clear_task_history_button_);
  storage_layout->addWidget(reset_window_state_button_);

  status_label_ = new QLabel(QStringLiteral("就绪"), this);

  root_layout->addWidget(task_group);
  root_layout->addWidget(storage_group);
  root_layout->addWidget(status_label_);
  root_layout->addStretch(1);

  connect(apply_button_, &QPushButton::clicked, this, &SettingsPage::ApplyTaskRetention);
  connect(clear_recent_button_, &QPushButton::clicked, this, &SettingsPage::ClearRecentHistory);
  connect(clear_task_history_button_, &QPushButton::clicked, this, &SettingsPage::ClearTaskHistory);
  connect(reset_window_state_button_, &QPushButton::clicked, this, &SettingsPage::ResetWindowState);
}

void SettingsPage::ApplyTaskRetention() {
  if (task_manager_ == nullptr || settings_ == nullptr) {
    status_label_->setText(QStringLiteral("设置服务不可用"));
    return;
  }

  const int max_completed = max_completed_tasks_spin_->value();
  const int max_concurrent = max_concurrent_tasks_spin_->value();
  const QString report_directory = report_directory_picker_ != nullptr ? report_directory_picker_->Path() : QString();
  task_manager_->SetMaxCompletedTasks(max_completed);
  task_manager_->SetMaxConcurrentTasks(max_concurrent);
  settings_->WriteValue(QStringLiteral("settings/max_completed_tasks"), max_completed);
  settings_->WriteValue(QStringLiteral("settings/max_concurrent_tasks"), max_concurrent);
  settings_->WriteValue(QStringLiteral("settings/report_directory"), report_directory);
  emit ReportDirectoryChanged(report_directory);
  status_label_->setText(
      QStringLiteral("已应用：完成任务上限=%1，并发任务上限=%2").arg(max_completed).arg(max_concurrent));
}

void SettingsPage::ClearRecentHistory() {
  if (settings_ == nullptr) {
    status_label_->setText(QStringLiteral("设置服务不可用"));
    return;
  }

  settings_->ClearByPrefix(QStringLiteral("recent"));
  status_label_->setText(QStringLiteral("最近参数/文件历史已清空"));
}

void SettingsPage::ClearTaskHistory() {
  if (settings_ == nullptr) {
    status_label_->setText(QStringLiteral("设置服务不可用"));
    return;
  }

  settings_->ClearTaskHistory();
  if (task_manager_ != nullptr) {
    task_manager_->ClearRetainedTasks();
  }
  emit TaskHistoryCleared();
  status_label_->setText(QStringLiteral("任务历史记录已清空"));
}

void SettingsPage::ResetWindowState() {
  if (settings_ == nullptr) {
    status_label_->setText(QStringLiteral("设置服务不可用"));
    return;
  }

  settings_->RemoveKey(QStringLiteral("ui/main_window_geometry"));
  settings_->RemoveKey(QStringLiteral("ui/main_window_state"));
  status_label_->setText(QStringLiteral("窗口布局状态已重置（重启生效）"));
}

}  // namespace pdftools_gui::pages
