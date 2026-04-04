#include "pdftools_gui/pages/attachments_page.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/services/validation_service.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace pdftools_gui::pages {

AttachmentsPage::AttachmentsPage(pdftools_gui::services::TaskManager* task_manager,
                                 pdftools_gui::services::SettingsService* settings,
                                 QWidget* parent)
    : QWidget(parent), task_manager_(task_manager), settings_(settings) {
  auto* root_layout = new QVBoxLayout(this);

  auto* form_group = new QGroupBox(QStringLiteral("附件提取参数"), this);
  auto* form_layout = new QFormLayout(form_group);

  input_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  input_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kOpenFile);
  input_picker_->SetDialogTitle(QStringLiteral("选择输入 PDF"));
  input_picker_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  input_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("attachments/input")));
  const QString recent_input = settings_->RecentValue(QStringLiteral("attachments/input"));
  if (!recent_input.isEmpty()) {
    input_picker_->SetPath(recent_input);
  }

  output_dir_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  output_dir_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kDirectory);
  output_dir_picker_->SetDialogTitle(QStringLiteral("选择附件输出目录"));
  output_dir_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("attachments/output")));
  const QString recent_output = settings_->RecentValue(QStringLiteral("attachments/output"));
  if (!recent_output.isEmpty()) {
    output_dir_picker_->SetPath(recent_output);
  }

  strict_checkbox_ = new QCheckBox(QStringLiteral("严格模式（解析失败直接报错）"), form_group);
  overwrite_checkbox_ = new QCheckBox(QStringLiteral("覆盖同名附件"), form_group);
  strict_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("attachments/strict"), false).toBool());
  overwrite_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("attachments/overwrite"), false).toBool());

  form_layout->addRow(QStringLiteral("输入 PDF"), input_picker_);
  form_layout->addRow(QStringLiteral("输出目录"), output_dir_picker_);
  form_layout->addRow(QString(), strict_checkbox_);
  form_layout->addRow(QString(), overwrite_checkbox_);

  run_button_ = new QPushButton(QStringLiteral("开始提取附件"), this);
  task_label_ = new QLabel(QStringLiteral("尚未提交任务"), this);
  result_view_ = new QPlainTextEdit(this);
  result_view_->setReadOnly(true);

  root_layout->addWidget(form_group);
  root_layout->addWidget(run_button_);
  root_layout->addWidget(task_label_);
  root_layout->addWidget(result_view_, 1);

  connect(run_button_, &QPushButton::clicked, this, &AttachmentsPage::SubmitTask);
  connect(task_manager_, &pdftools_gui::services::TaskManager::TaskCompleted, this, &AttachmentsPage::HandleTaskCompleted);
}

void AttachmentsPage::SubmitTask() {
  const QString input = input_picker_->Path();
  const QString output_dir = output_dir_picker_->Path();
  const bool overwrite = overwrite_checkbox_->isChecked();

  QString error = pdftools_gui::services::ValidationService::ValidateExistingFile(input, QStringLiteral("输入 PDF"));
  if (error.isEmpty()) {
    error = pdftools_gui::services::ValidationService::ValidateDirectory(output_dir, QStringLiteral("输出目录"));
  }
  if (!error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("参数错误"), error);
    return;
  }

  pdftools::pdf::ExtractAttachmentsRequest request;
  request.input_pdf = input.toStdString();
  request.output_dir = output_dir.toStdString();
  request.best_effort = !strict_checkbox_->isChecked();
  request.overwrite = overwrite;

  settings_->PushRecentValue(QStringLiteral("attachments/input"), input);
  settings_->PushRecentValue(QStringLiteral("attachments/output"), output_dir);
  settings_->WriteValue(QStringLiteral("attachments/strict"), strict_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("attachments/overwrite"), overwrite);
  settings_->SetLastDirectory(QStringLiteral("attachments"), output_dir);

  last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kExtractAttachments,
                                        QStringLiteral("Extract Attachments"),
                                        pdftools_gui::services::RequestVariant{request});
  task_label_->setText(QStringLiteral("已提交任务 #%1").arg(last_task_id_));
  result_view_->setPlainText(QStringLiteral("任务正在执行..."));
}

void AttachmentsPage::HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info) {
  if (info.id != last_task_id_) {
    return;
  }

  task_label_->setText(QStringLiteral("任务 #%1 已完成 (%2)")
                           .arg(info.id)
                           .arg(pdftools_gui::services::TaskStateToDisplayString(info.state)));
  result_view_->setPlainText(info.summary + QStringLiteral("\n") + info.detail);
}

}  // namespace pdftools_gui::pages
