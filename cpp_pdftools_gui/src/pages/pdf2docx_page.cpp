#include "pdftools_gui/pages/pdf2docx_page.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPixmap>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
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

Pdf2DocxPage::Pdf2DocxPage(pdftools_gui::services::TaskManager* task_manager,
                           pdftools_gui::services::SettingsService* settings,
                           QWidget* parent)
    : QWidget(parent), task_manager_(task_manager), settings_(settings) {
  preview_service_ = std::make_unique<pdftools_gui::services::PreviewService>();
  auto* root_layout = new QVBoxLayout(this);

  auto* form_group = new QGroupBox(QStringLiteral("PDF -> DOCX 参数"), this);
  auto* form_layout = new QFormLayout(form_group);

  input_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  input_picker_->setObjectName(QStringLiteral("pdf2docx_input_picker"));
  input_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kOpenFile);
  input_picker_->SetDialogTitle(QStringLiteral("选择输入 PDF"));
  input_picker_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  input_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("pdf2docx/input")));
  const QString recent_input = settings_->RecentValue(QStringLiteral("pdf2docx/input"));
  if (!recent_input.isEmpty()) {
    input_picker_->SetPath(recent_input);
  }

  input_info_label_ = new QLabel(QStringLiteral("当前 PDF：未选择文件"), form_group);
  input_info_label_->setObjectName(QStringLiteral("pdf2docx_input_info_label"));

  output_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  output_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kSaveFile);
  output_picker_->SetDialogTitle(QStringLiteral("选择输出 DOCX"));
  output_picker_->SetNameFilter(QStringLiteral("Word Files (*.docx)"));
  output_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("pdf2docx/output")));
  const QString recent_output = settings_->RecentValue(QStringLiteral("pdf2docx/output"));
  if (!recent_output.isEmpty()) {
    output_picker_->SetPath(recent_output);
  }

  enable_dump_ir_checkbox_ = new QCheckBox(QStringLiteral("同时导出 IR JSON"), form_group);

  dump_ir_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  dump_ir_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kSaveFile);
  dump_ir_picker_->SetDialogTitle(QStringLiteral("选择 IR JSON 输出路径"));
  dump_ir_picker_->SetNameFilter(QStringLiteral("JSON Files (*.json)"));
  dump_ir_picker_->SetPlaceholderText(QStringLiteral("可选，例如: /path/to/dump_ir.json"));
  dump_ir_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("pdf2docx/dump_ir")));
  const QString recent_dump_ir = settings_->RecentValue(QStringLiteral("pdf2docx/dump_ir"));
  if (!recent_dump_ir.isEmpty()) {
    dump_ir_picker_->SetPath(recent_dump_ir);
  }

  extract_images_checkbox_ = new QCheckBox(QStringLiteral("提取图片"), form_group);
  extract_images_checkbox_->setChecked(true);
  anchored_images_checkbox_ = new QCheckBox(QStringLiteral("DOCX 使用 anchored 图片"), form_group);
  font_fallback_checkbox_ = new QCheckBox(QStringLiteral("启用字体 fallback"), form_group);
  font_fallback_checkbox_->setChecked(true);
  overwrite_checkbox_ = new QCheckBox(QStringLiteral("允许覆盖输出"), form_group);

  enable_dump_ir_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("pdf2docx/enable_dump_ir"), false).toBool());
  extract_images_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("pdf2docx/extract_images"), true).toBool());
  anchored_images_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("pdf2docx/anchored_images"), false).toBool());
  font_fallback_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("pdf2docx/font_fallback"), true).toBool());
  overwrite_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("pdf2docx/overwrite"), false).toBool());

  form_layout->addRow(QStringLiteral("输入 PDF"), input_picker_);
  form_layout->addRow(QStringLiteral("输入 PDF 信息"), input_info_label_);
  form_layout->addRow(QStringLiteral("输出 DOCX"), output_picker_);
  form_layout->addRow(QString(), enable_dump_ir_checkbox_);
  form_layout->addRow(QStringLiteral("IR JSON"), dump_ir_picker_);
  form_layout->addRow(QString(), extract_images_checkbox_);
  form_layout->addRow(QString(), anchored_images_checkbox_);
  form_layout->addRow(QString(), font_fallback_checkbox_);
  form_layout->addRow(QString(), overwrite_checkbox_);

  auto* preview_group = new QGroupBox(QStringLiteral("IR 预览与提图预览"), this);
  auto* preview_layout = new QFormLayout(preview_group);
  preview_page_spin_ = new QSpinBox(preview_group);
  preview_page_spin_->setObjectName(QStringLiteral("pdf2docx_preview_page_spin"));
  preview_page_spin_->setRange(0, 1000000);
  preview_page_spin_->setSpecialValueText(QStringLiteral("0 (all pages)"));
  preview_page_spin_->setValue(1);
  preview_refresh_button_ = new QPushButton(QStringLiteral("刷新 IR 预览"), preview_group);
  preview_refresh_button_->setObjectName(QStringLiteral("pdf2docx_preview_refresh_button"));
  preview_summary_label_ = new QLabel(QStringLiteral("预览未刷新"), preview_group);
  preview_summary_label_->setObjectName(QStringLiteral("pdf2docx_preview_summary_label"));
  preview_browser_ = new QTextBrowser(preview_group);
  preview_browser_->setObjectName(QStringLiteral("pdf2docx_preview_browser"));
  preview_browser_->setMinimumHeight(220);
  preview_images_list_ = new QListWidget(preview_group);
  preview_images_list_->setObjectName(QStringLiteral("pdf2docx_preview_images_list"));
  preview_images_list_->setViewMode(QListView::IconMode);
  preview_images_list_->setIconSize(QSize(120, 120));
  preview_images_list_->setResizeMode(QListView::Adjust);
  preview_images_list_->setSpacing(8);
  preview_images_list_->setMinimumHeight(180);

  preview_layout->addRow(QStringLiteral("预览页码"), preview_page_spin_);
  preview_layout->addRow(QString(), preview_refresh_button_);
  preview_layout->addRow(QString(), preview_summary_label_);
  preview_layout->addRow(QStringLiteral("IR 预览"), preview_browser_);
  preview_layout->addRow(QStringLiteral("提取图片预览"), preview_images_list_);

  run_button_ = new QPushButton(QStringLiteral("开始转换"), this);
  task_label_ = new QLabel(QStringLiteral("尚未提交任务"), this);
  result_view_ = new QPlainTextEdit(this);
  result_view_->setReadOnly(true);

  root_layout->addWidget(form_group);
  root_layout->addWidget(preview_group);
  root_layout->addWidget(run_button_);
  root_layout->addWidget(task_label_);
  root_layout->addWidget(result_view_, 1);

  connect(enable_dump_ir_checkbox_, &QCheckBox::toggled, this, &Pdf2DocxPage::UpdateDumpIrEnabled);
  connect(run_button_, &QPushButton::clicked, this, &Pdf2DocxPage::SubmitTask);
  connect(task_manager_, &pdftools_gui::services::TaskManager::TaskCompleted, this, &Pdf2DocxPage::HandleTaskCompleted);
  connect(input_picker_, &pdftools_gui::widgets::PathPicker::PathChanged, this, [this](const QString&) {
    UpdateInputPdfInfo();
  });
  connect(preview_refresh_button_, &QPushButton::clicked, this, &Pdf2DocxPage::RefreshIrPreview);

  UpdateDumpIrEnabled(enable_dump_ir_checkbox_->isChecked());
  UpdateInputPdfInfo();
}

void Pdf2DocxPage::UpdateInputPdfInfo() {
  if (preview_service_ == nullptr) {
    input_info_label_->setText(QStringLiteral("预览服务不可用"));
    return;
  }
  QString error;
  const int pages = preview_service_->QueryPdfPageCount(input_picker_->Path(), &error);
  if (pages <= 0) {
    input_info_label_->setText(error.isEmpty() ? QStringLiteral("无法读取当前 PDF") : QStringLiteral("无法读取：%1").arg(error));
    preview_page_spin_->setRange(0, 1000000);
    preview_images_list_->clear();
    preview_summary_label_->setText(QStringLiteral("预览未刷新"));
    preview_browser_->clear();
    return;
  }

  input_info_label_->setText(QStringLiteral("页数：%1").arg(pages));
  preview_page_spin_->setRange(0, pages);
  if (preview_page_spin_->value() > pages) {
    preview_page_spin_->setValue(pages);
  }
}

void Pdf2DocxPage::RefreshIrPreview() {
  if (preview_service_ == nullptr) {
    preview_summary_label_->setText(QStringLiteral("预览服务不可用"));
    return;
  }

  const int preview_page = preview_page_spin_->value();
  const bool include_images = extract_images_checkbox_->isChecked();
  const auto preview = preview_service_->RenderPdfIrPreview(
      input_picker_->Path(), preview_page, include_images, 1.0, 700, 80);
  if (!preview.success) {
    preview_summary_label_->setText(preview.message);
    preview_browser_->setHtml(QStringLiteral("<html><body><p>%1</p></body></html>").arg(preview.message.toHtmlEscaped()));
    preview_images_list_->clear();
    return;
  }
  preview_summary_label_->setText(
      QStringLiteral("预览完成：总页数=%1，渲染页数=%2，文本块=%3，图片=%4")
          .arg(preview.total_pages)
          .arg(preview.rendered_pages)
          .arg(preview.span_count)
          .arg(preview.image_count));
  preview_browser_->setHtml(preview.html);

  QString image_error;
  const auto thumbs = preview_service_->ExtractImageThumbnailsFromPdf(
      input_picker_->Path(), preview_page, 120, 24, &image_error);
  preview_images_list_->clear();
  for (const auto& thumb : thumbs) {
    auto* item =
        new QListWidgetItem(QIcon(QPixmap::fromImage(thumb.image)), thumb.label, preview_images_list_);
    item->setToolTip(thumb.label);
    preview_images_list_->addItem(item);
  }
  if (!image_error.isEmpty()) {
    preview_summary_label_->setText(preview_summary_label_->text() + QStringLiteral(" | 图片预览错误：%1").arg(image_error));
  }
}

void Pdf2DocxPage::SubmitTask() {
  const QString input = input_picker_->Path();
  const QString output = output_picker_->Path();
  const bool overwrite = overwrite_checkbox_->isChecked();

  QString error = pdftools_gui::services::ValidationService::ValidateExistingFile(input, QStringLiteral("输入 PDF"));
  if (error.isEmpty()) {
    error = pdftools_gui::services::ValidationService::ValidateOutputFile(output, overwrite, QStringLiteral("输出 DOCX"));
  }

  QString dump_ir;
  if (error.isEmpty() && enable_dump_ir_checkbox_->isChecked()) {
    dump_ir = dump_ir_picker_->Path();
    error = pdftools_gui::services::ValidationService::ValidateOutputFile(dump_ir, overwrite, QStringLiteral("IR JSON 输出"));
  }

  if (!error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("参数错误"), error);
    return;
  }

  pdftools::convert::PdfToDocxRequest request;
  request.input_pdf = input.toStdString();
  request.output_docx = output.toStdString();
  request.dump_ir_json_path = dump_ir.toStdString();
  request.extract_images = extract_images_checkbox_->isChecked();
  request.enable_font_fallback = font_fallback_checkbox_->isChecked();
  request.use_anchored_images = anchored_images_checkbox_->isChecked();
  request.overwrite = overwrite;

  settings_->PushRecentValue(QStringLiteral("pdf2docx/input"), input);
  settings_->PushRecentValue(QStringLiteral("pdf2docx/output"), output);
  if (!dump_ir.isEmpty()) {
    settings_->PushRecentValue(QStringLiteral("pdf2docx/dump_ir"), dump_ir);
  }
  settings_->WriteValue(QStringLiteral("pdf2docx/enable_dump_ir"), enable_dump_ir_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("pdf2docx/extract_images"), extract_images_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("pdf2docx/anchored_images"), anchored_images_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("pdf2docx/font_fallback"), font_fallback_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("pdf2docx/overwrite"), overwrite);
  settings_->SetLastDirectory(QStringLiteral("pdf2docx"), output);

  last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kPdfToDocx,
                                        QStringLiteral("PDF To DOCX"),
                                        pdftools_gui::services::RequestVariant{request});
  task_label_->setText(QStringLiteral("已提交任务 #%1").arg(last_task_id_));
  result_view_->setPlainText(QStringLiteral("任务正在执行..."));
}

void Pdf2DocxPage::HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info) {
  if (info.id != last_task_id_) {
    return;
  }

  task_label_->setText(QStringLiteral("任务 #%1 已完成 (%2)")
                           .arg(info.id)
                           .arg(pdftools_gui::services::TaskStateToDisplayString(info.state)));
  result_view_->setPlainText(info.summary + QStringLiteral("\n") + info.detail);
}

void Pdf2DocxPage::UpdateDumpIrEnabled(bool enabled) {
  dump_ir_picker_->setEnabled(enabled);
}

}  // namespace pdftools_gui::pages
