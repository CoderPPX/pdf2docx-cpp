#include "pdftools_gui/pages/text_extract_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "pdftools_gui/services/preview_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/services/validation_service.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace pdftools_gui::pages {

TextExtractPage::TextExtractPage(pdftools_gui::services::TaskManager* task_manager,
                                 pdftools_gui::services::SettingsService* settings,
                                 QWidget* parent)
    : QWidget(parent), task_manager_(task_manager), settings_(settings) {
  preview_service_ = std::make_unique<pdftools_gui::services::PreviewService>();
  auto* root_layout = new QVBoxLayout(this);

  auto* form_group = new QGroupBox(QStringLiteral("文本提取参数"), this);
  auto* form_layout = new QFormLayout(form_group);

  input_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  input_picker_->setObjectName(QStringLiteral("text_extract_input_picker"));
  input_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kOpenFile);
  input_picker_->SetDialogTitle(QStringLiteral("选择输入 PDF"));
  input_picker_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  input_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("text_extract/input")));
  const QString recent_input = settings_->RecentValue(QStringLiteral("text_extract/input"));
  if (!recent_input.isEmpty()) {
    input_picker_->SetPath(recent_input);
  }

  input_info_label_ = new QLabel(QStringLiteral("当前 PDF：未选择文件"), form_group);
  input_info_label_->setObjectName(QStringLiteral("text_extract_input_info_label"));

  output_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  output_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kSaveFile);
  output_picker_->SetDialogTitle(QStringLiteral("选择输出文本文件"));
  output_picker_->SetNameFilter(QStringLiteral("Text/JSON Files (*.txt *.json)"));
  output_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("text_extract/output")));
  const QString recent_output = settings_->RecentValue(QStringLiteral("text_extract/output"));
  if (!recent_output.isEmpty()) {
    output_picker_->SetPath(recent_output);
  }

  format_combo_ = new QComboBox(form_group);
  format_combo_->addItem(QStringLiteral("Plain Text (.txt)"), static_cast<int>(pdftools::pdf::TextOutputFormat::kPlainText));
  format_combo_->addItem(QStringLiteral("JSON (.json)"), static_cast<int>(pdftools::pdf::TextOutputFormat::kJson));

  page_start_spin_ = new QSpinBox(form_group);
  page_start_spin_->setObjectName(QStringLiteral("text_extract_page_start_spin"));
  page_start_spin_->setRange(1, 1000000);
  page_start_spin_->setValue(1);

  page_end_spin_ = new QSpinBox(form_group);
  page_end_spin_->setObjectName(QStringLiteral("text_extract_page_end_spin"));
  page_end_spin_->setRange(0, 1000000);
  page_end_spin_->setSpecialValueText(QStringLiteral("0 (to last page)"));
  page_end_spin_->setValue(0);

  include_positions_checkbox_ = new QCheckBox(QStringLiteral("包含位置信息"), form_group);
  strict_checkbox_ = new QCheckBox(QStringLiteral("严格模式（禁用 fallback）"), form_group);
  overwrite_checkbox_ = new QCheckBox(QStringLiteral("允许覆盖输出"), form_group);

  page_start_spin_->setValue(settings_->ReadValue(QStringLiteral("text_extract/page_start"), 1).toInt());
  page_end_spin_->setValue(settings_->ReadValue(QStringLiteral("text_extract/page_end"), 0).toInt());
  include_positions_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("text_extract/include_positions"), false).toBool());
  strict_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("text_extract/strict"), false).toBool());
  overwrite_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("text_extract/overwrite"), false).toBool());
  format_combo_->setCurrentIndex(settings_->ReadValue(QStringLiteral("text_extract/format_index"), 0).toInt());

  form_layout->addRow(QStringLiteral("输入 PDF"), input_picker_);
  form_layout->addRow(QStringLiteral("输入 PDF 信息"), input_info_label_);
  form_layout->addRow(QStringLiteral("输出文件"), output_picker_);
  form_layout->addRow(QStringLiteral("输出格式"), format_combo_);
  form_layout->addRow(QStringLiteral("起始页"), page_start_spin_);
  form_layout->addRow(QStringLiteral("结束页"), page_end_spin_);
  form_layout->addRow(QString(), include_positions_checkbox_);
  form_layout->addRow(QString(), strict_checkbox_);
  form_layout->addRow(QString(), overwrite_checkbox_);

  auto* preview_group = new QGroupBox(QStringLiteral("当前 PDF 预览 (IR)"), this);
  auto* preview_layout = new QFormLayout(preview_group);
  preview_page_spin_ = new QSpinBox(preview_group);
  preview_page_spin_->setObjectName(QStringLiteral("text_extract_preview_page_spin"));
  preview_page_spin_->setRange(1, 1000000);
  preview_page_spin_->setValue(1);
  preview_refresh_button_ = new QPushButton(QStringLiteral("刷新预览"), preview_group);
  preview_refresh_button_->setObjectName(QStringLiteral("text_extract_preview_refresh_button"));
  preview_summary_label_ = new QLabel(QStringLiteral("预览未刷新"), preview_group);
  preview_summary_label_->setObjectName(QStringLiteral("text_extract_preview_summary_label"));
  preview_browser_ = new QTextBrowser(preview_group);
  preview_browser_->setObjectName(QStringLiteral("text_extract_preview_browser"));
  preview_browser_->setMinimumHeight(220);
  preview_browser_->setPlaceholderText(QStringLiteral("点击“刷新预览”显示选中页的 IR 预览。"));
  preview_layout->addRow(QStringLiteral("预览页码"), preview_page_spin_);
  preview_layout->addRow(QString(), preview_refresh_button_);
  preview_layout->addRow(QString(), preview_summary_label_);
  preview_layout->addRow(QString(), preview_browser_);

  run_button_ = new QPushButton(QStringLiteral("开始提取"), this);
  task_label_ = new QLabel(QStringLiteral("尚未提交任务"), this);
  result_view_ = new QPlainTextEdit(this);
  result_view_->setReadOnly(true);

  root_layout->addWidget(form_group);
  root_layout->addWidget(preview_group);
  root_layout->addWidget(run_button_);
  root_layout->addWidget(task_label_);
  root_layout->addWidget(result_view_, 1);

  connect(run_button_, &QPushButton::clicked, this, &TextExtractPage::SubmitTask);
  connect(task_manager_, &pdftools_gui::services::TaskManager::TaskCompleted, this, &TextExtractPage::HandleTaskCompleted);
  connect(input_picker_, &pdftools_gui::widgets::PathPicker::PathChanged, this, [this](const QString&) {
    UpdateInputPdfInfo();
  });
  connect(preview_refresh_button_, &QPushButton::clicked, this, &TextExtractPage::RefreshPreview);

  UpdateInputPdfInfo();
}

void TextExtractPage::UpdateInputPdfInfo() {
  if (preview_service_ == nullptr) {
    input_info_label_->setText(QStringLiteral("预览服务不可用"));
    return;
  }

  QString error;
  const int page_count = preview_service_->QueryPdfPageCount(input_picker_->Path(), &error);
  if (page_count <= 0) {
    input_info_label_->setText(error.isEmpty() ? QStringLiteral("无法读取当前 PDF") : QStringLiteral("无法读取：%1").arg(error));
    page_start_spin_->setRange(1, 1000000);
    page_end_spin_->setRange(0, 1000000);
    preview_page_spin_->setRange(1, 1000000);
    preview_summary_label_->setText(QStringLiteral("预览未刷新"));
    preview_browser_->clear();
    return;
  }

  input_info_label_->setText(QStringLiteral("页数：%1").arg(page_count));
  page_start_spin_->setRange(1, page_count);
  page_end_spin_->setRange(0, page_count);
  preview_page_spin_->setRange(1, page_count);
  if (preview_page_spin_->value() > page_count) {
    preview_page_spin_->setValue(page_count);
  }
}

void TextExtractPage::RefreshPreview() {
  if (preview_service_ == nullptr) {
    preview_summary_label_->setText(QStringLiteral("预览服务不可用"));
    return;
  }

  const auto result = preview_service_->RenderPdfIrPreview(
      input_picker_->Path(), preview_page_spin_->value(), false, 1.1, 500, 0);
  if (!result.success) {
    preview_summary_label_->setText(result.message);
    preview_browser_->setHtml(QStringLiteral("<html><body><p>%1</p></body></html>").arg(result.message.toHtmlEscaped()));
    return;
  }

  preview_summary_label_->setText(
      QStringLiteral("预览完成：总页数=%1，渲染页数=%2，文本块=%3")
          .arg(result.total_pages)
          .arg(result.rendered_pages)
          .arg(result.span_count));
  preview_browser_->setHtml(result.html);
}

void TextExtractPage::SubmitTask() {
  const QString input = input_picker_->Path();
  const QString output = output_picker_->Path();
  const bool overwrite = overwrite_checkbox_->isChecked();

  QString error = pdftools_gui::services::ValidationService::ValidateExistingFile(input, QStringLiteral("输入 PDF"));
  if (error.isEmpty()) {
    error = pdftools_gui::services::ValidationService::ValidateOutputFile(output, overwrite, QStringLiteral("输出文件"));
  }
  if (!error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("参数错误"), error);
    return;
  }

  pdftools::pdf::ExtractTextRequest request;
  request.input_pdf = input.toStdString();
  request.output_path = output.toStdString();
  request.page_start = static_cast<uint32_t>(page_start_spin_->value());
  request.page_end = static_cast<uint32_t>(page_end_spin_->value());
  request.include_positions = include_positions_checkbox_->isChecked();
  request.output_format = static_cast<pdftools::pdf::TextOutputFormat>(format_combo_->currentData().toInt());
  request.best_effort = !strict_checkbox_->isChecked();
  request.overwrite = overwrite;

  settings_->PushRecentValue(QStringLiteral("text_extract/input"), input);
  settings_->PushRecentValue(QStringLiteral("text_extract/output"), output);
  settings_->WriteValue(QStringLiteral("text_extract/page_start"), page_start_spin_->value());
  settings_->WriteValue(QStringLiteral("text_extract/page_end"), page_end_spin_->value());
  settings_->WriteValue(QStringLiteral("text_extract/include_positions"), include_positions_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("text_extract/strict"), strict_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("text_extract/overwrite"), overwrite);
  settings_->WriteValue(QStringLiteral("text_extract/format_index"), format_combo_->currentIndex());
  settings_->SetLastDirectory(QStringLiteral("text_extract"), output);

  last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kExtractText,
                                        QStringLiteral("Extract Text"),
                                        pdftools_gui::services::RequestVariant{request});
  task_label_->setText(QStringLiteral("已提交任务 #%1").arg(last_task_id_));
  result_view_->setPlainText(QStringLiteral("任务正在执行..."));
}

void TextExtractPage::HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info) {
  if (info.id != last_task_id_) {
    return;
  }

  task_label_->setText(QStringLiteral("任务 #%1 已完成 (%2)")
                           .arg(info.id)
                           .arg(pdftools_gui::services::TaskStateToDisplayString(info.state)));
  result_view_->setPlainText(info.summary + QStringLiteral("\n") + info.detail);
}

}  // namespace pdftools_gui::pages
