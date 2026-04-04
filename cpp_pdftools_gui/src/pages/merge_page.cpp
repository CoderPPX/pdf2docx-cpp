#include "pdftools_gui/pages/merge_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "pdftools/pdf/document_ops.hpp"
#include "pdftools_gui/services/preview_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/services/validation_service.hpp"
#include "pdftools_gui/widgets/file_list_editor.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace pdftools_gui::pages {

MergePage::MergePage(pdftools_gui::services::TaskManager* task_manager,
                     pdftools_gui::services::SettingsService* settings,
                     QWidget* parent)
    : QWidget(parent), task_manager_(task_manager), settings_(settings) {
  preview_service_ = std::make_unique<pdftools_gui::services::PreviewService>();
  auto* root_layout = new QVBoxLayout(this);

  auto* input_group = new QGroupBox(QStringLiteral("输入 PDF 列表"), this);
  auto* input_layout = new QVBoxLayout(input_group);
  input_files_ = new pdftools_gui::widgets::FileListEditor(input_group);
  input_files_->setObjectName(QStringLiteral("merge_input_files"));
  input_files_->SetDialogTitle(QStringLiteral("选择要合并的 PDF"));
  input_files_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  input_summary_label_ = new QLabel(QStringLiteral("已选择 0 个文件，预计总页数 0"), input_group);
  input_summary_label_->setObjectName(QStringLiteral("merge_input_summary_label"));
  input_layout->addWidget(input_files_);
  input_layout->addWidget(input_summary_label_);

  auto* preview_group = new QGroupBox(QStringLiteral("当前 PDF 预览 (IR)"), this);
  auto* preview_layout = new QFormLayout(preview_group);
  preview_pdf_combo_ = new QComboBox(preview_group);
  preview_pdf_combo_->setObjectName(QStringLiteral("merge_preview_pdf_combo"));
  preview_page_spin_ = new QSpinBox(preview_group);
  preview_page_spin_->setObjectName(QStringLiteral("merge_preview_page_spin"));
  preview_page_spin_->setRange(1, 1000000);
  preview_page_spin_->setValue(1);
  preview_refresh_button_ = new QPushButton(QStringLiteral("刷新预览"), preview_group);
  preview_refresh_button_->setObjectName(QStringLiteral("merge_preview_refresh_button"));
  preview_summary_label_ = new QLabel(QStringLiteral("预览未刷新"), preview_group);
  preview_summary_label_->setObjectName(QStringLiteral("merge_preview_summary_label"));
  preview_browser_ = new QTextBrowser(preview_group);
  preview_browser_->setObjectName(QStringLiteral("merge_preview_browser"));
  preview_browser_->setMinimumHeight(220);
  preview_browser_->setPlaceholderText(QStringLiteral("选择一个输入 PDF 后可预览指定页。"));
  preview_layout->addRow(QStringLiteral("预览文件"), preview_pdf_combo_);
  preview_layout->addRow(QStringLiteral("预览页码"), preview_page_spin_);
  preview_layout->addRow(QString(), preview_refresh_button_);
  preview_layout->addRow(QString(), preview_summary_label_);
  preview_layout->addRow(QString(), preview_browser_);

  auto* form_group = new QGroupBox(QStringLiteral("输出与参数"), this);
  auto* form_layout = new QFormLayout(form_group);
  output_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  output_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kSaveFile);
  output_picker_->SetDialogTitle(QStringLiteral("选择输出 PDF"));
  output_picker_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  output_picker_->SetPlaceholderText(QStringLiteral("例如: /path/to/output.pdf"));

  const QStringList recent_outputs = settings_->RecentValues(QStringLiteral("merge/output"));
  output_picker_->SetSuggestions(recent_outputs);
  if (!recent_outputs.isEmpty()) {
    output_picker_->SetPath(recent_outputs.front());
  }

  const QStringList stored_inputs =
      settings_->ReadValue(QStringLiteral("merge/inputs_last"), QStringList()).toStringList();
  QStringList usable_inputs;
  for (const QString& path : stored_inputs) {
    QFileInfo info(path);
    if (info.exists() && info.isFile()) {
      usable_inputs.push_back(path);
    }
  }
  if (!usable_inputs.isEmpty()) {
    input_files_->SetPaths(usable_inputs);
  }
  RefreshInputSummary(input_files_->Paths());
  RefreshPreviewSourceList(input_files_->Paths());

  overwrite_checkbox_ = new QCheckBox(QStringLiteral("允许覆盖已存在文件"), form_group);

  form_layout->addRow(QStringLiteral("输出文件"), output_picker_);
  form_layout->addRow(QString(), overwrite_checkbox_);

  run_button_ = new QPushButton(QStringLiteral("开始合并"), this);
  task_label_ = new QLabel(QStringLiteral("尚未提交任务"), this);
  result_view_ = new QPlainTextEdit(this);
  result_view_->setReadOnly(true);
  result_view_->setPlaceholderText(QStringLiteral("任务结果会显示在这里。"));

  root_layout->addWidget(input_group);
  root_layout->addWidget(preview_group);
  root_layout->addWidget(form_group);
  root_layout->addWidget(run_button_);
  root_layout->addWidget(task_label_);
  root_layout->addWidget(result_view_, 1);

  connect(run_button_, &QPushButton::clicked, this, &MergePage::SubmitTask);
  connect(task_manager_, &pdftools_gui::services::TaskManager::TaskCompleted, this, &MergePage::HandleTaskCompleted);
  connect(input_files_,
          &pdftools_gui::widgets::FileListEditor::PathsChanged,
          this,
          [this](const QStringList& paths) {
            RefreshInputSummary(paths);
            RefreshPreviewSourceList(paths);
          });
  connect(preview_pdf_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
    QString error;
    const int pages = preview_service_->QueryPdfPageCount(preview_pdf_combo_->currentData().toString(), &error);
    if (pages > 0) {
      preview_page_spin_->setRange(1, pages);
      if (preview_page_spin_->value() > pages) {
        preview_page_spin_->setValue(pages);
      }
    } else {
      preview_page_spin_->setRange(1, 1000000);
      preview_summary_label_->setText(error.isEmpty() ? QStringLiteral("预览未刷新") : QStringLiteral("无法读取：%1").arg(error));
    }
  });
  connect(preview_refresh_button_, &QPushButton::clicked, this, &MergePage::RefreshCurrentPdfPreview);
}

void MergePage::RefreshInputSummary(const QStringList& paths) {
  uint32_t total_pages = 0;
  int unreadable_count = 0;
  for (const QString& path : paths) {
    pdftools::pdf::PdfInfoRequest request;
    request.input_pdf = path.toStdString();
    pdftools::pdf::PdfInfoResult result;
    const pdftools::Status status = pdftools::pdf::GetPdfInfo(request, &result);
    if (!status.ok()) {
      ++unreadable_count;
      continue;
    }
    total_pages += result.page_count;
  }

  QString summary =
      QStringLiteral("已选择 %1 个文件，预计总页数 %2").arg(paths.size()).arg(total_pages);
  if (unreadable_count > 0) {
    summary += QStringLiteral("（无法读取 %1 个）").arg(unreadable_count);
  }
  input_summary_label_->setText(summary);
}

void MergePage::RefreshPreviewSourceList(const QStringList& paths) {
  preview_pdf_combo_->blockSignals(true);
  preview_pdf_combo_->clear();
  for (const QString& path : paths) {
    preview_pdf_combo_->addItem(QFileInfo(path).fileName(), path);
  }
  preview_pdf_combo_->blockSignals(false);

  if (preview_pdf_combo_->count() <= 0) {
    preview_page_spin_->setRange(1, 1000000);
    preview_summary_label_->setText(QStringLiteral("预览未刷新"));
    preview_browser_->clear();
    return;
  }

  preview_pdf_combo_->setCurrentIndex(0);
  QString error;
  const int pages = preview_service_->QueryPdfPageCount(preview_pdf_combo_->currentData().toString(), &error);
  if (pages > 0) {
    preview_page_spin_->setRange(1, pages);
    if (preview_page_spin_->value() > pages) {
      preview_page_spin_->setValue(pages);
    }
  } else {
    preview_page_spin_->setRange(1, 1000000);
    preview_summary_label_->setText(error.isEmpty() ? QStringLiteral("预览未刷新") : QStringLiteral("无法读取：%1").arg(error));
    preview_browser_->clear();
  }
}

void MergePage::RefreshCurrentPdfPreview() {
  if (preview_service_ == nullptr) {
    preview_summary_label_->setText(QStringLiteral("预览服务不可用"));
    return;
  }
  const QString source_path = preview_pdf_combo_->currentData().toString();
  if (source_path.isEmpty()) {
    preview_summary_label_->setText(QStringLiteral("请先添加输入 PDF"));
    preview_browser_->clear();
    return;
  }

  const auto preview = preview_service_->RenderPdfIrPreview(
      source_path, preview_page_spin_->value(), true, 1.0, 500, 40);
  if (!preview.success) {
    preview_summary_label_->setText(preview.message);
    preview_browser_->setHtml(QStringLiteral("<html><body><p>%1</p></body></html>").arg(preview.message.toHtmlEscaped()));
    return;
  }

  preview_summary_label_->setText(
      QStringLiteral("预览完成：总页数=%1，渲染页数=%2，文本块=%3，图片=%4")
          .arg(preview.total_pages)
          .arg(preview.rendered_pages)
          .arg(preview.span_count)
          .arg(preview.image_count));
  preview_browser_->setHtml(preview.html);
}

void MergePage::SubmitTask() {
  const QStringList files = input_files_->Paths();
  const QString output = output_picker_->Path();
  const bool overwrite = overwrite_checkbox_->isChecked();

  QString error = pdftools_gui::services::ValidationService::ValidateNonEmptyList(files, QStringLiteral("输入 PDF"));
  if (error.isEmpty()) {
    for (const QString& file : files) {
      error = pdftools_gui::services::ValidationService::ValidateExistingFile(file, QStringLiteral("输入 PDF"));
      if (!error.isEmpty()) {
        break;
      }
    }
  }
  if (error.isEmpty()) {
    error = pdftools_gui::services::ValidationService::ValidateOutputFile(output, overwrite, QStringLiteral("输出 PDF"));
  }
  if (!error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("参数错误"), error);
    return;
  }

  pdftools::pdf::MergePdfRequest request;
  request.overwrite = overwrite;
  request.output_pdf = output.toStdString();
  for (const QString& file : files) {
    request.input_pdfs.push_back(file.toStdString());
  }

  settings_->PushRecentValue(QStringLiteral("merge/input"), files.join(QStringLiteral(";")));
  for (const QString& file : files) {
    settings_->PushRecentValue(QStringLiteral("merge/input_file"), file);
  }
  settings_->PushRecentValue(QStringLiteral("merge/output"), output);
  settings_->WriteValue(QStringLiteral("merge/inputs_last"), files);
  settings_->SetLastDirectory(QStringLiteral("merge"), output);

  last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kMergePdf,
                                        QStringLiteral("Merge PDF"),
                                        pdftools_gui::services::RequestVariant{request});
  task_label_->setText(QStringLiteral("已提交任务 #%1").arg(last_task_id_));
  result_view_->setPlainText(QStringLiteral("任务正在执行..."));
}

void MergePage::HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info) {
  if (info.id != last_task_id_) {
    return;
  }
  task_label_->setText(QStringLiteral("任务 #%1 已完成 (%2)")
                           .arg(info.id)
                           .arg(pdftools_gui::services::TaskStateToDisplayString(info.state)));
  result_view_->setPlainText(info.summary + QStringLiteral("\n") + info.detail);
}

}  // namespace pdftools_gui::pages
