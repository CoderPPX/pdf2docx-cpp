#include "pdftools_gui/pages/image_to_pdf_page.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "pdftools_gui/services/preview_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/services/validation_service.hpp"
#include "pdftools_gui/widgets/file_list_editor.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace pdftools_gui::pages {

ImageToPdfPage::ImageToPdfPage(pdftools_gui::services::TaskManager* task_manager,
                               pdftools_gui::services::SettingsService* settings,
                               QWidget* parent)
    : QWidget(parent), task_manager_(task_manager), settings_(settings) {
  preview_service_ = std::make_unique<pdftools_gui::services::PreviewService>();
  auto* root_layout = new QVBoxLayout(this);

  auto* input_group = new QGroupBox(QStringLiteral("输入图片"), this);
  auto* input_layout = new QVBoxLayout(input_group);
  image_files_ = new pdftools_gui::widgets::FileListEditor(input_group);
  image_files_->setObjectName(QStringLiteral("image2pdf_input_files"));
  image_files_->SetDialogTitle(QStringLiteral("选择输入图片"));
  image_files_->SetNameFilter(QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp)"));
  image_preview_summary_label_ = new QLabel(QStringLiteral("预览：0 张可读图片"), input_group);
  image_preview_summary_label_->setObjectName(QStringLiteral("image2pdf_preview_summary_label"));
  image_preview_list_ = new QListWidget(input_group);
  image_preview_list_->setObjectName(QStringLiteral("image2pdf_preview_list"));
  image_preview_list_->setViewMode(QListView::IconMode);
  image_preview_list_->setResizeMode(QListView::Adjust);
  image_preview_list_->setSpacing(8);
  image_preview_list_->setIconSize(QSize(120, 120));
  image_preview_list_->setMinimumHeight(180);
  image_preview_list_->setMovement(QListView::Static);
  image_preview_list_->setSelectionMode(QAbstractItemView::SingleSelection);
  input_layout->addWidget(image_files_);
  input_layout->addWidget(image_preview_summary_label_);
  input_layout->addWidget(image_preview_list_);

  auto* form_group = new QGroupBox(QStringLiteral("输出与参数"), this);
  auto* form_layout = new QFormLayout(form_group);

  output_picker_ = new pdftools_gui::widgets::PathPicker(form_group);
  output_picker_->SetMode(pdftools_gui::widgets::PathPicker::Mode::kSaveFile);
  output_picker_->SetDialogTitle(QStringLiteral("选择输出 PDF"));
  output_picker_->SetNameFilter(QStringLiteral("PDF Files (*.pdf)"));
  output_picker_->SetSuggestions(settings_->RecentValues(QStringLiteral("image2pdf/output")));
  const QString recent_output = settings_->RecentValue(QStringLiteral("image2pdf/output"));
  if (!recent_output.isEmpty()) {
    output_picker_->SetPath(recent_output);
  }

  use_image_size_checkbox_ = new QCheckBox(QStringLiteral("页面尺寸跟随图片原始尺寸"), form_group);
  page_width_spin_ = new QDoubleSpinBox(form_group);
  page_width_spin_->setRange(1.0, 5000.0);
  page_width_spin_->setValue(595.0);
  page_width_spin_->setSuffix(QStringLiteral(" pt"));

  page_height_spin_ = new QDoubleSpinBox(form_group);
  page_height_spin_->setRange(1.0, 5000.0);
  page_height_spin_->setValue(842.0);
  page_height_spin_->setSuffix(QStringLiteral(" pt"));

  margin_spin_ = new QDoubleSpinBox(form_group);
  margin_spin_->setRange(0.0, 1000.0);
  margin_spin_->setValue(0.0);
  margin_spin_->setSuffix(QStringLiteral(" pt"));

  scale_combo_ = new QComboBox(form_group);
  scale_combo_->addItem(QStringLiteral("Fit"), static_cast<int>(pdftools::pdf::ImageScaleMode::kFit));
  scale_combo_->addItem(QStringLiteral("Original"), static_cast<int>(pdftools::pdf::ImageScaleMode::kOriginal));

  overwrite_checkbox_ = new QCheckBox(QStringLiteral("允许覆盖输出"), form_group);

  const QStringList stored_images =
      settings_->ReadValue(QStringLiteral("image2pdf/images_last"), QStringList()).toStringList();
  QStringList usable_images;
  for (const QString& path : stored_images) {
    QFileInfo info(path);
    if (info.exists() && info.isFile()) {
      usable_images.push_back(path);
    }
  }
  if (!usable_images.isEmpty()) {
    image_files_->SetPaths(usable_images);
  }
  RefreshImagePreview(image_files_->Paths());

  use_image_size_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("image2pdf/use_image_size"), false).toBool());
  page_width_spin_->setValue(settings_->ReadValue(QStringLiteral("image2pdf/page_width"), 595.0).toDouble());
  page_height_spin_->setValue(settings_->ReadValue(QStringLiteral("image2pdf/page_height"), 842.0).toDouble());
  margin_spin_->setValue(settings_->ReadValue(QStringLiteral("image2pdf/margin"), 0.0).toDouble());
  scale_combo_->setCurrentIndex(settings_->ReadValue(QStringLiteral("image2pdf/scale_index"), 0).toInt());
  overwrite_checkbox_->setChecked(settings_->ReadValue(QStringLiteral("image2pdf/overwrite"), false).toBool());

  form_layout->addRow(QStringLiteral("输出 PDF"), output_picker_);
  form_layout->addRow(QString(), use_image_size_checkbox_);
  form_layout->addRow(QStringLiteral("页面宽度"), page_width_spin_);
  form_layout->addRow(QStringLiteral("页面高度"), page_height_spin_);
  form_layout->addRow(QStringLiteral("边距"), margin_spin_);
  form_layout->addRow(QStringLiteral("缩放策略"), scale_combo_);
  form_layout->addRow(QString(), overwrite_checkbox_);

  run_button_ = new QPushButton(QStringLiteral("开始转换"), this);
  task_label_ = new QLabel(QStringLiteral("尚未提交任务"), this);
  result_view_ = new QPlainTextEdit(this);
  result_view_->setReadOnly(true);

  root_layout->addWidget(input_group);
  root_layout->addWidget(form_group);
  root_layout->addWidget(run_button_);
  root_layout->addWidget(task_label_);
  root_layout->addWidget(result_view_, 1);

  connect(use_image_size_checkbox_, &QCheckBox::toggled, this, &ImageToPdfPage::UpdatePageOptionsEnabled);
  connect(run_button_, &QPushButton::clicked, this, &ImageToPdfPage::SubmitTask);
  connect(task_manager_, &pdftools_gui::services::TaskManager::TaskCompleted, this, &ImageToPdfPage::HandleTaskCompleted);
  connect(image_files_, &pdftools_gui::widgets::FileListEditor::PathsChanged, this, &ImageToPdfPage::RefreshImagePreview);

  UpdatePageOptionsEnabled(use_image_size_checkbox_->isChecked());
}

void ImageToPdfPage::RefreshImagePreview(const QStringList& paths) {
  image_preview_list_->clear();
  if (preview_service_ == nullptr) {
    image_preview_summary_label_->setText(QStringLiteral("预览服务不可用"));
    return;
  }

  int unreadable_count = 0;
  const QVector<pdftools_gui::services::ImageThumbnail> thumbs =
      preview_service_->BuildImageThumbnails(paths, 120, &unreadable_count);
  for (const auto& thumb : thumbs) {
    auto* item = new QListWidgetItem(QIcon(QPixmap::fromImage(thumb.image)), thumb.label, image_preview_list_);
    item->setToolTip(thumb.label);
    image_preview_list_->addItem(item);
  }

  QString summary = QStringLiteral("预览：%1 张可读图片").arg(thumbs.size());
  if (unreadable_count > 0) {
    summary += QStringLiteral("，%1 张无法读取").arg(unreadable_count);
  }
  image_preview_summary_label_->setText(summary);
}

void ImageToPdfPage::SubmitTask() {
  const QStringList images = image_files_->Paths();
  const QString output = output_picker_->Path();
  const bool overwrite = overwrite_checkbox_->isChecked();

  QString error = pdftools_gui::services::ValidationService::ValidateNonEmptyList(images, QStringLiteral("输入图片"));
  if (error.isEmpty()) {
    for (const QString& image : images) {
      error = pdftools_gui::services::ValidationService::ValidateExistingFile(image, QStringLiteral("输入图片"));
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

  pdftools::pdf::ImagesToPdfRequest request;
  request.output_pdf = output.toStdString();
  request.use_image_size_as_page = use_image_size_checkbox_->isChecked();
  request.page_width_pt = page_width_spin_->value();
  request.page_height_pt = page_height_spin_->value();
  request.margin_pt = margin_spin_->value();
  request.scale_mode = static_cast<pdftools::pdf::ImageScaleMode>(scale_combo_->currentData().toInt());
  request.overwrite = overwrite;
  for (const QString& image : images) {
    request.image_paths.push_back(image.toStdString());
  }

  settings_->PushRecentValue(QStringLiteral("image2pdf/output"), output);
  for (const QString& image : images) {
    settings_->PushRecentValue(QStringLiteral("image2pdf/input_image"), image);
  }
  settings_->WriteValue(QStringLiteral("image2pdf/images_last"), images);
  settings_->WriteValue(QStringLiteral("image2pdf/use_image_size"), use_image_size_checkbox_->isChecked());
  settings_->WriteValue(QStringLiteral("image2pdf/page_width"), page_width_spin_->value());
  settings_->WriteValue(QStringLiteral("image2pdf/page_height"), page_height_spin_->value());
  settings_->WriteValue(QStringLiteral("image2pdf/margin"), margin_spin_->value());
  settings_->WriteValue(QStringLiteral("image2pdf/scale_index"), scale_combo_->currentIndex());
  settings_->WriteValue(QStringLiteral("image2pdf/overwrite"), overwrite);
  settings_->SetLastDirectory(QStringLiteral("image2pdf"), output);

  last_task_id_ = task_manager_->Submit(pdftools_gui::services::OperationKind::kImagesToPdf,
                                        QStringLiteral("Images To PDF"),
                                        pdftools_gui::services::RequestVariant{request});
  task_label_->setText(QStringLiteral("已提交任务 #%1").arg(last_task_id_));
  result_view_->setPlainText(QStringLiteral("任务正在执行..."));
}

void ImageToPdfPage::HandleTaskCompleted(const pdftools_gui::services::TaskInfo& info) {
  if (info.id != last_task_id_) {
    return;
  }

  task_label_->setText(QStringLiteral("任务 #%1 已完成 (%2)")
                           .arg(info.id)
                           .arg(pdftools_gui::services::TaskStateToDisplayString(info.state)));
  result_view_->setPlainText(info.summary + QStringLiteral("\n") + info.detail);
}

void ImageToPdfPage::UpdatePageOptionsEnabled(bool use_image_size) {
  page_width_spin_->setEnabled(!use_image_size);
  page_height_spin_->setEnabled(!use_image_size);
}

}  // namespace pdftools_gui::pages
