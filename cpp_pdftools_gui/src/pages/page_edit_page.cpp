#include "pdftools_gui/pages/page_edit_page.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

#include "pdftools/pdf/document_ops.hpp"
#include "pdftools_gui/services/preview_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/services/validation_service.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace pdftools_gui::pages {

namespace {

constexpr int kDefaultMaxPageValue = 1000000;

QWidget* BuildDeleteTab(QWidget* parent, QSpinBox** delete_page_spin) {
  auto* tab = new QWidget(parent);
  auto* layout = new QFormLayout(tab);

  auto* page_spin = new QSpinBox(tab);
  page_spin->setObjectName(QStringLiteral("page_edit_delete_page_spin"));
  page_spin->setRange(1, kDefaultMaxPageValue);
  page_spin->setValue(1);

  layout->addRow(QStringLiteral("删除页号"), page_spin);
  *delete_page_spin = page_spin;
  return tab;
}

QWidget* BuildInsertTab(QWidget* parent,
                        QSpinBox** at_spin,
                        pdftools_gui::widgets::PathPicker** source_picker,
                        QLabel** source_info_label,
                        QSpinBox** source_page_spin) {
  auto* tab = new QWidget(parent);
  auto* layout = new QFormLayout(tab);

  auto* insert_at = new QSpinBox(tab);
  insert_at->setObjectName(QStringLiteral("page_edit_insert_at_spin"));
  insert_at->setRange(1, kDefaultMaxPageValue);
  insert_at->setValue(1);

  auto* source = new pdftools_gui::widgets::PathPicker(tab);
  source->SetMode(pdftools_gui::widgets::PathPicker::Mode::kOpenFile);
  source->SetDialogTitle(QStringLiteral("选择来源 PDF"));
  source->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  source->setObjectName(QStringLiteral("page_edit_insert_source_picker"));

  auto* source_info = new QLabel(QStringLiteral("来源 PDF：未选择文件"), tab);
  source_info->setObjectName(QStringLiteral("page_edit_insert_source_info_label"));

  auto* source_page = new QSpinBox(tab);
  source_page->setObjectName(QStringLiteral("page_edit_insert_source_page_spin"));
  source_page->setRange(1, kDefaultMaxPageValue);
  source_page->setValue(1);

  layout->addRow(QStringLiteral("插入位置 (1-based)"), insert_at);
  layout->addRow(QStringLiteral("来源 PDF"), source);
  layout->addRow(QStringLiteral("来源 PDF 信息"), source_info);
  layout->addRow(QStringLiteral("来源页号"), source_page);

  *at_spin = insert_at;
  *source_picker = source;
  *source_info_label = source_info;
  *source_page_spin = source_page;
  return tab;
}

QWidget* BuildReplaceTab(QWidget* parent,
                         QSpinBox** page_spin,
                         pdftools_gui::widgets::PathPicker** source_picker,
                         QLabel** source_info_label,
                         QSpinBox** source_page_spin) {
  auto* tab = new QWidget(parent);
  auto* layout = new QFormLayout(tab);

  auto* page = new QSpinBox(tab);
  page->setObjectName(QStringLiteral("page_edit_replace_page_spin"));
  page->setRange(1, kDefaultMaxPageValue);
  page->setValue(1);

  auto* source = new pdftools_gui::widgets::PathPicker(tab);
  source->SetMode(pdftools_gui::widgets::PathPicker::Mode::kOpenFile);
  source->SetDialogTitle(QStringLiteral("选择来源 PDF"));
  source->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  source->setObjectName(QStringLiteral("page_edit_replace_source_picker"));

  auto* source_info = new QLabel(QStringLiteral("来源 PDF：未选择文件"), tab);
  source_info->setObjectName(QStringLiteral("page_edit_replace_source_info_label"));

  auto* source_page = new QSpinBox(tab);
  source_page->setObjectName(QStringLiteral("page_edit_replace_source_page_spin"));
  source_page->setRange(1, kDefaultMaxPageValue);
  source_page->setValue(1);

  layout->addRow(QStringLiteral("替换页号"), page);
  layout->addRow(QStringLiteral("来源 PDF"), source);
  layout->addRow(QStringLiteral("来源 PDF 信息"), source_info);
  layout->addRow(QStringLiteral("来源页号"), source_page);

  *page_spin = page;
  *source_picker = source;
  *source_info_label = source_info;
  *source_page_spin = source_page;
  return tab;
}

}  // namespace

PageEditPage::PageEditPage(pdftools_gui::services::TaskManager* task_manager,
                           pdftools_gui::services::SettingsService* settings,
                           QWidget* parent)
    : QWidget(parent), task_manager_(task_manager), settings_(settings) {
  preview_service_ = std::make_unique<pdftools_gui::services::PreviewService>();
  auto* root_layout = new QVBoxLayout(this);

  auto* common_group = new QGroupBox(QStringLiteral("公共参数"), this);
  auto* common_layout = new QFormLayout(common_group);

  input_picker_ = new pdftools_gui::widgets::PathPicker(common_group);
  input_picker_->setObjectName(QStringLiteral("page_edit_input_picker"));
  input_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kOpenFile);
  input_picker_->SetDialogTitle(QStringLiteral("选择目标 PDF"));
  input_picker_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  input_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("page_edit/input")));
  const QString recent_input = settings_->RecentValue(QStringLiteral("page_edit/input"));
  if (!recent_input.isEmpty()) {
    input_picker_->SetPath(recent_input);
  }

  input_info_label_ = new QLabel(QStringLiteral("目标 PDF：未选择文件"), common_group);
  input_info_label_->setObjectName(QStringLiteral("page_edit_input_info_label"));

  output_picker_ = new pdftools_gui::widgets::PathPicker(common_group);
  output_picker_->setObjectName(QStringLiteral("page_edit_output_picker"));
  output_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kSaveFile);
  output_picker_->SetDialogTitle(QStringLiteral("选择输出 PDF"));
  output_picker_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  output_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("page_edit/output")));
  const QString recent_output = settings_->RecentValue(QStringLiteral("page_edit/output"));
  if (!recent_output.isEmpty()) {
    output_picker_->SetPath(recent_output);
  }

  overwrite_checkbox_ = new QCheckBox(QStringLiteral("允许覆盖输出"), common_group);

  common_layout->addRow(QStringLiteral("输入 PDF"), input_picker_);
  common_layout->addRow(QStringLiteral("输入 PDF 信息"), input_info_label_);
  common_layout->addRow(QStringLiteral("输出 PDF"), output_picker_);
  common_layout->addRow(QString(), overwrite_checkbox_);

  tab_widget_ = new QTabWidget(this);
  tab_widget_->addTab(BuildDeleteTab(tab_widget_, &delete_page_spin_), QStringLiteral("删除页面"));
  tab_widget_->addTab(
      BuildInsertTab(tab_widget_,
                     &insert_at_spin_,
                     &insert_source_picker_,
                     &insert_source_info_label_,
                     &insert_source_page_spin_),
      QStringLiteral("插入页面"));
  tab_widget_->addTab(
      BuildReplaceTab(tab_widget_,
                      &replace_page_spin_,
                      &replace_source_picker_,
                      &replace_source_info_label_,
                      &replace_source_page_spin_),
      QStringLiteral("替换页面"));

  insert_source_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("page_edit/source")));
  replace_source_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("page_edit/source")));
  const QString recent_source = settings_->RecentValue(QStringLiteral("page_edit/source"));
  if (!recent_source.isEmpty()) {
    insert_source_picker_->SetPath(recent_source);
    replace_source_picker_->SetPath(recent_source);
  }

  overwrite_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("page_edit/overwrite"), false).toBool());
  delete_page_spin_->setValue(settings_->ReadValue(QStringLiteral("page_edit/delete_page"), 1).toInt());
  insert_at_spin_->setValue(settings_->ReadValue(QStringLiteral("page_edit/insert_at"), 1).toInt());
  insert_source_page_spin_->setValue(settings_->ReadValue(QStringLiteral("page_edit/insert_source_page"), 1).toInt());
  replace_page_spin_->setValue(settings_->ReadValue(QStringLiteral("page_edit/replace_page"), 1).toInt());
  replace_source_page_spin_->setValue(settings_->ReadValue(QStringLiteral("page_edit/replace_source_page"), 1).toInt());
  tab_widget_->setCurrentIndex(settings_->ReadValue(QStringLiteral("page_edit/tab_index"), 0).toInt());

  run_button_ = new QPushButton(QStringLiteral("执行页面操作"), this);
  preview_refresh_button_ = new QPushButton(QStringLiteral("刷新页预览"), this);
  preview_refresh_button_->setObjectName(QStringLiteral("page_edit_preview_refresh_button"));
  preview_summary_label_ = new QLabel(QStringLiteral("预览未刷新"), this);
  preview_summary_label_->setObjectName(QStringLiteral("page_edit_preview_summary_label"));
  preview_browser_ = new QTextBrowser(this);
  preview_browser_->setObjectName(QStringLiteral("page_edit_preview_browser"));
  preview_browser_->setMinimumHeight(220);
  task_label_ = new QLabel(QStringLiteral("尚未提交任务"), this);
  result_view_ = new QPlainTextEdit(this);
  result_view_->setReadOnly(true);

  root_layout->addWidget(common_group);
  root_layout->addWidget(tab_widget_);
  root_layout->addWidget(run_button_);
  root_layout->addWidget(preview_refresh_button_);
  root_layout->addWidget(preview_summary_label_);
  root_layout->addWidget(preview_browser_, 1);
  root_layout->addWidget(task_label_);
  root_layout->addWidget(result_view_, 1);

  RefreshPdfInfo();

  connect(run_button_, &QPushButton::clicked, this, &PageEditPage::SubmitTask);
  connect(task_manager_, &pdftools_gui::services::TaskManager::TaskCompleted, this, &PageEditPage::HandleTaskCompleted);
  connect(preview_refresh_button_, &QPushButton::clicked, this, &PageEditPage::RefreshSelectedPagePreview);
  connect(input_picker_, &pdftools_gui::widgets::PathPicker::PathChanged, this, [this](const QString&) {
    RefreshPdfInfo();
  });
  connect(insert_source_picker_, &pdftools_gui::widgets::PathPicker::PathChanged, this, [this](const QString&) {
    RefreshPdfInfo();
  });
  connect(replace_source_picker_, &pdftools_gui::widgets::PathPicker::PathChanged, this, [this](const QString&) {
    RefreshPdfInfo();
  });

  connect(tab_widget_, &QTabWidget::currentChanged, this, [this](int) {
    preview_summary_label_->setText(QStringLiteral("参数已变更，点击“刷新页预览”"));
  });
  connect(delete_page_spin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
    preview_summary_label_->setText(QStringLiteral("参数已变更，点击“刷新页预览”"));
  });
  connect(insert_source_page_spin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
    preview_summary_label_->setText(QStringLiteral("参数已变更，点击“刷新页预览”"));
  });
  connect(replace_source_page_spin_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
    preview_summary_label_->setText(QStringLiteral("参数已变更，点击“刷新页预览”"));
  });
}

void PageEditPage::RefreshPdfInfo() {
  const uint32_t input_page_count =
      RefreshPdfInfoForLabel(input_picker_ != nullptr ? input_picker_->Path() : QString(), input_info_label_);
  const uint32_t insert_source_page_count =
      RefreshPdfInfoForLabel(insert_source_picker_ != nullptr ? insert_source_picker_->Path() : QString(),
                             insert_source_info_label_);
  const uint32_t replace_source_page_count =
      RefreshPdfInfoForLabel(replace_source_picker_ != nullptr ? replace_source_picker_->Path() : QString(),
                             replace_source_info_label_);

  if (input_page_count > 0) {
    delete_page_spin_->setRange(1, static_cast<int>(input_page_count));
    replace_page_spin_->setRange(1, static_cast<int>(input_page_count));
    insert_at_spin_->setRange(1, static_cast<int>(input_page_count + 1));
  } else {
    delete_page_spin_->setRange(1, kDefaultMaxPageValue);
    replace_page_spin_->setRange(1, kDefaultMaxPageValue);
    insert_at_spin_->setRange(1, kDefaultMaxPageValue);
  }

  if (insert_source_page_count > 0) {
    insert_source_page_spin_->setRange(1, static_cast<int>(insert_source_page_count));
  } else {
    insert_source_page_spin_->setRange(1, kDefaultMaxPageValue);
  }

  if (replace_source_page_count > 0) {
    replace_source_page_spin_->setRange(1, static_cast<int>(replace_source_page_count));
  } else {
    replace_source_page_spin_->setRange(1, kDefaultMaxPageValue);
  }
}

uint32_t PageEditPage::RefreshPdfInfoForLabel(const QString& path, QLabel* label) {
  if (label == nullptr) {
    return 0;
  }
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    label->setText(QStringLiteral("未选择文件"));
    return 0;
  }

  pdftools::pdf::PdfInfoRequest request;
  request.input_pdf = trimmed.toStdString();
  pdftools::pdf::PdfInfoResult result;
  const pdftools::Status status = pdftools::pdf::GetPdfInfo(request, &result);
  if (!status.ok()) {
    label->setText(QStringLiteral("无法读取：%1").arg(QString::fromStdString(status.message())));
    return 0;
  }

  label->setText(QStringLiteral("页数：%1").arg(result.page_count));
  return result.page_count;
}

void PageEditPage::RefreshSelectedPagePreview() {
  if (preview_service_ == nullptr) {
    preview_summary_label_->setText(QStringLiteral("预览服务不可用"));
    return;
  }

  QString source_path;
  int page_number = 1;
  const int tab_index = tab_widget_->currentIndex();
  if (tab_index == 0) {
    source_path = input_picker_->Path();
    page_number = delete_page_spin_->value();
  } else if (tab_index == 1) {
    source_path = insert_source_picker_->Path();
    page_number = insert_source_page_spin_->value();
  } else {
    source_path = replace_source_picker_->Path();
    page_number = replace_source_page_spin_->value();
  }

  const auto preview = preview_service_->RenderPdfIrPreview(source_path, page_number, true, 1.0, 500, 40);
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

void PageEditPage::SubmitTask() {
  const QString input = input_picker_->Path();
  const QString output = output_picker_->Path();
  const bool overwrite = overwrite_checkbox_->isChecked();

  QString error = pdftools_gui::services::ValidationService::ValidateExistingFile(input, QStringLiteral("输入 PDF"));
  if (error.isEmpty()) {
    error = pdftools_gui::services::ValidationService::ValidateOutputFile(output, overwrite, QStringLiteral("输出 PDF"));
  }

  if (!error.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("参数错误"), error);
    return;
  }

  const int tab_index = tab_widget_->currentIndex();

  if (tab_index == 0) {
    const int page = delete_page_spin_->value();
    error = pdftools_gui::services::ValidationService::ValidatePageNumber(page, QStringLiteral("删除页号"));
    if (!error.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("参数错误"), error);
      return;
    }

    pdftools::pdf::DeletePageRequest request;
    request.input_pdf = input.toStdString();
    request.output_pdf = output.toStdString();
    request.page_number = static_cast<uint32_t>(page);
    request.overwrite = overwrite;

    last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kDeletePage,
                                          QStringLiteral("Delete Page"),
                                          pdftools_gui::services::RequestVariant{request});
  } else if (tab_index == 1) {
    const QString source = insert_source_picker_->Path();
    const int at = insert_at_spin_->value();
    const int source_page = insert_source_page_spin_->value();

    error = pdftools_gui::services::ValidationService::ValidateExistingFile(source, QStringLiteral("来源 PDF"));
    if (error.isEmpty()) {
      error = pdftools_gui::services::ValidationService::ValidatePageNumber(at, QStringLiteral("插入位置"));
    }
    if (error.isEmpty()) {
      error = pdftools_gui::services::ValidationService::ValidatePageNumber(source_page, QStringLiteral("来源页号"));
    }
    if (!error.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("参数错误"), error);
      return;
    }

    pdftools::pdf::InsertPageRequest request;
    request.input_pdf = input.toStdString();
    request.output_pdf = output.toStdString();
    request.insert_at = static_cast<uint32_t>(at);
    request.source_pdf = source.toStdString();
    request.source_page_number = static_cast<uint32_t>(source_page);
    request.overwrite = overwrite;

    last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kInsertPage,
                                          QStringLiteral("Insert Page"),
                                          pdftools_gui::services::RequestVariant{request});
  } else {
    const QString source = replace_source_picker_->Path();
    const int page = replace_page_spin_->value();
    const int source_page = replace_source_page_spin_->value();

    error = pdftools_gui::services::ValidationService::ValidateExistingFile(source, QStringLiteral("来源 PDF"));
    if (error.isEmpty()) {
      error = pdftools_gui::services::ValidationService::ValidatePageNumber(page, QStringLiteral("替换页号"));
    }
    if (error.isEmpty()) {
      error = pdftools_gui::services::ValidationService::ValidatePageNumber(source_page, QStringLiteral("来源页号"));
    }
    if (!error.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("参数错误"), error);
      return;
    }

    pdftools::pdf::ReplacePageRequest request;
    request.input_pdf = input.toStdString();
    request.output_pdf = output.toStdString();
    request.page_number = static_cast<uint32_t>(page);
    request.source_pdf = source.toStdString();
    request.source_page_number = static_cast<uint32_t>(source_page);
    request.overwrite = overwrite;

    last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kReplacePage,
                                          QStringLiteral("Replace Page"),
                                          pdftools_gui::services::RequestVariant{request});
  }

  settings_->PushRecentValue(QStringLiteral("page_edit/input"), input);
  settings_->PushRecentValue(QStringLiteral("page_edit/output"), output);
  if (tab_index == 1) {
    settings_->PushRecentValue(QStringLiteral("page_edit/source"), insert_source_picker_->Path());
  } else if (tab_index == 2) {
    settings_->PushRecentValue(QStringLiteral("page_edit/source"), replace_source_picker_->Path());
  }
  settings_->WriteValue(QStringLiteral("page_edit/overwrite"), overwrite);
  settings_->WriteValue(QStringLiteral("page_edit/delete_page"), delete_page_spin_->value());
  settings_->WriteValue(QStringLiteral("page_edit/insert_at"), insert_at_spin_->value());
  settings_->WriteValue(QStringLiteral("page_edit/insert_source_page"), insert_source_page_spin_->value());
  settings_->WriteValue(QStringLiteral("page_edit/replace_page"), replace_page_spin_->value());
  settings_->WriteValue(QStringLiteral("page_edit/replace_source_page"), replace_source_page_spin_->value());
  settings_->WriteValue(QStringLiteral("page_edit/tab_index"), tab_index);
  settings_->SetLastDirectory(QStringLiteral("page_edit"), output);

  task_label_->setText(QStringLiteral("已提交任务 #%1").arg(last_task_id_));
  result_view_->setPlainText(QStringLiteral("任务正在执行..."));
}

void PageEditPage::HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info) {
  if (info.id != last_task_id_) {
    return;
  }

  task_label_->setText(QStringLiteral("任务 #%1 已完成 (%2)")
                           .arg(info.id)
                           .arg(pdftools_gui::services::TaskStateToDisplayString(info.state)));
  result_view_->setPlainText(info.summary + QStringLiteral("\n") + info.detail);
}

}  // namespace pdftools_gui::pages
