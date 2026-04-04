#include "pdftools_gui/widgets/task_center_widget.hpp"

#include <optional>

#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

#include "pdftools_gui/models/task_item_model.hpp"

namespace pdftools_gui::widgets {

namespace {

class TaskFilterProxyModel : public QSortFilterProxyModel {
 public:
  explicit TaskFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

  void SetFilterState(std::optional<pdftools_gui::services::TaskState> state) {
    filter_state_ = state;
    invalidateFilter();
  }

  void SetKeyword(QString keyword) {
    keyword_ = keyword.trimmed();
    invalidateFilter();
  }

 protected:
  bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override {
    if (sourceModel() == nullptr) {
      return true;
    }

    if (filter_state_.has_value()) {
      const QModelIndex state_index =
          sourceModel()->index(source_row, pdftools_gui::models::TaskItemModel::kStatus, source_parent);
      const QVariant state_value =
          sourceModel()->data(state_index, pdftools_gui::models::TaskItemModel::kTaskStateRole);
      if (state_value.isValid() && state_value.toInt() != static_cast<int>(*filter_state_)) {
        return false;
      }
    }

    if (keyword_.isEmpty()) {
      return true;
    }

    const QModelIndex summary_index =
        sourceModel()->index(source_row, pdftools_gui::models::TaskItemModel::kSummary, source_parent);
    const QString summary = sourceModel()->data(summary_index, Qt::DisplayRole).toString();
    const QString detail =
        sourceModel()->data(summary_index, pdftools_gui::models::TaskItemModel::kDetailRole).toString();
    const QString haystack = summary + QStringLiteral("\n") + detail;
    return haystack.contains(keyword_, Qt::CaseInsensitive);
  }

 private:
  std::optional<pdftools_gui::services::TaskState> filter_state_;
  QString keyword_;
};

}  // namespace

TaskCenterWidget::TaskCenterWidget(QWidget* parent) : QWidget(parent) {
  filter_combo_ = new QComboBox(this);
  filter_combo_->setObjectName(QStringLiteral("task_filter_combo"));
  filter_combo_->addItem(QStringLiteral("All"));
  filter_combo_->addItem(QStringLiteral("Queued"));
  filter_combo_->addItem(QStringLiteral("Running"));
  filter_combo_->addItem(QStringLiteral("Succeeded"));
  filter_combo_->addItem(QStringLiteral("Failed"));
  filter_combo_->addItem(QStringLiteral("Canceled"));

  search_edit_ = new QLineEdit(this);
  search_edit_->setObjectName(QStringLiteral("task_search_edit"));
  search_edit_->setPlaceholderText(QStringLiteral("Search summary/detail..."));

  cancel_task_button_ = new QPushButton(QStringLiteral("Cancel Task"), this);
  cancel_task_button_->setObjectName(QStringLiteral("cancel_task_button"));
  export_report_button_ = new QPushButton(QStringLiteral("Export Report"), this);
  export_report_button_->setObjectName(QStringLiteral("export_report_button"));
  open_output_button_ = new QPushButton(QStringLiteral("Open Output"), this);
  open_output_button_->setObjectName(QStringLiteral("open_output_button"));
  table_view_ = new QTableView(this);
  detail_view_ = new QPlainTextEdit(this);
  detail_view_->setReadOnly(true);
  detail_view_->setPlaceholderText(QStringLiteral("选择任务后显示详细信息。"));

  cancel_task_button_->setEnabled(false);
  export_report_button_->setEnabled(false);
  open_output_button_->setEnabled(false);

  proxy_model_ = new TaskFilterProxyModel(this);
  table_view_->setModel(proxy_model_);
  table_view_->setAlternatingRowColors(true);
  table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_view_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_view_->setSortingEnabled(true);
  table_view_->horizontalHeader()->setStretchLastSection(true);

  auto* controls_layout = new QHBoxLayout();
  controls_layout->addWidget(filter_combo_);
  controls_layout->addWidget(search_edit_, 1);
  controls_layout->addWidget(cancel_task_button_);
  controls_layout->addWidget(export_report_button_);
  controls_layout->addWidget(open_output_button_);

  auto* root_layout = new QVBoxLayout(this);
  root_layout->addLayout(controls_layout);
  root_layout->addWidget(table_view_, 1);
  root_layout->addWidget(detail_view_, 1);

  connect(filter_combo_, &QComboBox::currentIndexChanged, this, &TaskCenterWidget::ApplyFilter);
  connect(search_edit_, &QLineEdit::textChanged, this, &TaskCenterWidget::ApplyKeywordFilter);
  connect(cancel_task_button_, &QPushButton::clicked, this, &TaskCenterWidget::CancelSelectedTask);
  connect(export_report_button_, &QPushButton::clicked, this, &TaskCenterWidget::ExportSelectedReport);
  connect(open_output_button_, &QPushButton::clicked, this, &TaskCenterWidget::OpenSelectedOutput);
  connect(proxy_model_, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex&, const QModelIndex&) {
    UpdateDetailPanel();
    UpdateActionButtons();
  });
}

void TaskCenterWidget::SetTaskManager(pdftools_gui::services::TaskManager* task_manager) {
  task_manager_ = task_manager;
}

void TaskCenterWidget::SetSourceModel(pdftools_gui::models::TaskItemModel* model) {
  proxy_model_->setSourceModel(model);
  table_view_->resizeColumnsToContents();
  if (table_view_->selectionModel() != nullptr) {
    connect(table_view_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex&, const QModelIndex&) {
              UpdateDetailPanel();
              UpdateActionButtons();
            });
  }
  UpdateActionButtons();
}

void TaskCenterWidget::SetReportDirectory(const QString& report_directory) {
  report_directory_ = report_directory;
}

QString TaskCenterWidget::LastExportedReportPath() const {
  return last_exported_report_path_;
}

void TaskCenterWidget::ApplyFilter(int index) {
  auto* filter = static_cast<TaskFilterProxyModel*>(proxy_model_);
  switch (index) {
    case 0:
      filter->SetFilterState(std::nullopt);
      break;
    case 1:
      filter->SetFilterState(pdftools_gui::services::TaskState::kQueued);
      break;
    case 2:
      filter->SetFilterState(pdftools_gui::services::TaskState::kRunning);
      break;
    case 3:
      filter->SetFilterState(pdftools_gui::services::TaskState::kSucceeded);
      break;
    case 4:
      filter->SetFilterState(pdftools_gui::services::TaskState::kFailed);
      break;
    case 5:
      filter->SetFilterState(pdftools_gui::services::TaskState::kCanceled);
      break;
    default:
      filter->SetFilterState(std::nullopt);
      break;
  }
}

void TaskCenterWidget::ApplyKeywordFilter(const QString& keyword) {
  auto* filter = static_cast<TaskFilterProxyModel*>(proxy_model_);
  filter->SetKeyword(keyword);
}

void TaskCenterWidget::CancelSelectedTask() {
  if (task_manager_ == nullptr) {
    QMessageBox::warning(this, QStringLiteral("Task Manager Missing"), QStringLiteral("任务管理器不可用。"));
    return;
  }

  const QModelIndex current = table_view_->currentIndex();
  if (!current.isValid()) {
    QMessageBox::information(this, QStringLiteral("No Task"), QStringLiteral("请先选择一个任务。"));
    return;
  }
  if (proxy_model_->sourceModel() == nullptr) {
    QMessageBox::warning(this, QStringLiteral("Model Missing"), QStringLiteral("任务模型不可用。"));
    return;
  }

  const QModelIndex source_index = proxy_model_->mapToSource(current);
  const QModelIndex id_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kId);
  const qint64 task_id =
      proxy_model_->sourceModel()->data(id_index, pdftools_gui::models::TaskItemModel::kTaskIdRole).toLongLong();

  if (!task_manager_->CancelTask(task_id)) {
    QMessageBox::information(this,
                             QStringLiteral("Cancel Rejected"),
                             QStringLiteral("仅支持取消排队中的任务。"));
  }
}

void TaskCenterWidget::ExportSelectedReport() {
  const QModelIndex current = table_view_->currentIndex();
  if (!current.isValid()) {
    QMessageBox::information(this, QStringLiteral("No Task"), QStringLiteral("请先选择一个任务。"));
    return;
  }

  if (proxy_model_->sourceModel() == nullptr) {
    QMessageBox::warning(this, QStringLiteral("Model Missing"), QStringLiteral("任务模型不可用。"));
    return;
  }

  const QModelIndex source_index = proxy_model_->mapToSource(current);
  const QModelIndex id_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kId);
  const QModelIndex operation_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kOperation);
  const QModelIndex status_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kStatus);
  const QModelIndex submitted_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kSubmittedAt);
  const QModelIndex finished_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kFinishedAt);
  const QModelIndex summary_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kSummary);

  const qint64 task_id = proxy_model_->sourceModel()->data(id_index, pdftools_gui::models::TaskItemModel::kTaskIdRole).toLongLong();
  const QString operation = proxy_model_->sourceModel()->data(operation_index, Qt::DisplayRole).toString();
  const QString status = proxy_model_->sourceModel()->data(status_index, Qt::DisplayRole).toString();
  const QString submitted = proxy_model_->sourceModel()->data(submitted_index, Qt::DisplayRole).toString();
  const QString finished = proxy_model_->sourceModel()->data(finished_index, Qt::DisplayRole).toString();
  const QString summary = proxy_model_->sourceModel()->data(summary_index, Qt::DisplayRole).toString();
  const QString detail =
      proxy_model_->sourceModel()->data(summary_index, pdftools_gui::models::TaskItemModel::kDetailRole).toString();
  const QString output_path =
      proxy_model_->sourceModel()->data(summary_index, pdftools_gui::models::TaskItemModel::kOutputPathRole).toString();

  QDir report_dir(ResolveReportDirectory());
  if (!report_dir.exists() && !report_dir.mkpath(QStringLiteral("."))) {
    QMessageBox::warning(this,
                         QStringLiteral("Export Failed"),
                         QStringLiteral("无法创建报告目录: %1").arg(report_dir.absolutePath()));
    return;
  }

  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
  const QString report_filename = QStringLiteral("task_%1_%2.txt").arg(task_id).arg(timestamp);
  const QString report_path = report_dir.filePath(report_filename);

  QFile file(report_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this,
                         QStringLiteral("Export Failed"),
                         QStringLiteral("无法写入报告文件: %1").arg(report_path));
    return;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << "PDFTools GUI Task Report\n";
  stream << "GeneratedAt: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
  stream << "TaskId: " << task_id << "\n";
  stream << "Operation: " << operation << "\n";
  stream << "Status: " << status << "\n";
  stream << "SubmittedAt: " << submitted << "\n";
  stream << "FinishedAt: " << finished << "\n";
  stream << "OutputPath: " << output_path << "\n";
  stream << "\n[Summary]\n" << summary << "\n";
  stream << "\n[Detail]\n" << detail << "\n";
  file.close();

  last_exported_report_path_ = report_path;
}

void TaskCenterWidget::OpenSelectedOutput() {
  const QModelIndex current = table_view_->currentIndex();
  if (!current.isValid()) {
    QMessageBox::information(this, QStringLiteral("No Task"), QStringLiteral("请先选择一个任务。"));
    return;
  }

  const QModelIndex source_index = proxy_model_->mapToSource(current);
  if (proxy_model_->sourceModel() == nullptr) {
    QMessageBox::warning(this, QStringLiteral("Model Missing"), QStringLiteral("任务模型不可用。"));
    return;
  }
  const QModelIndex output_index =
      source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kSummary);
  const QString output_path =
      proxy_model_->sourceModel()->data(output_index, pdftools_gui::models::TaskItemModel::kOutputPathRole).toString();

  if (output_path.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("No Output"), QStringLiteral("该任务没有输出路径。"));
    return;
  }

  QFileInfo info(output_path);
  QString open_target = output_path;
  if (!info.exists() && info.dir().exists()) {
    open_target = info.dir().absolutePath();
  }

  const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(open_target));
  if (!opened) {
    QMessageBox::warning(this, QStringLiteral("Open Failed"), QStringLiteral("无法打开输出路径: %1").arg(open_target));
  }
}

void TaskCenterWidget::UpdateDetailPanel() {
  const QModelIndex current = table_view_->currentIndex();
  if (!current.isValid() || proxy_model_->sourceModel() == nullptr) {
    detail_view_->clear();
    return;
  }

  const QModelIndex source_index = proxy_model_->mapToSource(current);
  const QModelIndex summary_index =
      source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kSummary);

  const QString summary = proxy_model_->sourceModel()->data(summary_index, Qt::DisplayRole).toString();
  const QString detail =
      proxy_model_->sourceModel()->data(summary_index, pdftools_gui::models::TaskItemModel::kDetailRole).toString();
  const QString output_path =
      proxy_model_->sourceModel()->data(summary_index, pdftools_gui::models::TaskItemModel::kOutputPathRole).toString();

  QString text = QStringLiteral("Summary:\n%1\n\nDetail:\n%2").arg(summary, detail);
  if (!output_path.isEmpty()) {
    text += QStringLiteral("\n\nOutput:\n%1").arg(output_path);
  }
  detail_view_->setPlainText(text);
}

void TaskCenterWidget::UpdateActionButtons() {
  const QModelIndex current = table_view_->currentIndex();
  const bool has_selection = current.isValid() && proxy_model_->sourceModel() != nullptr;

  export_report_button_->setEnabled(has_selection);
  open_output_button_->setEnabled(has_selection);

  bool can_cancel = false;
  if (has_selection) {
    const QModelIndex source_index = proxy_model_->mapToSource(current);
    const QModelIndex status_index = source_index.siblingAtColumn(pdftools_gui::models::TaskItemModel::kStatus);
    const int state_value =
        proxy_model_->sourceModel()->data(status_index, pdftools_gui::models::TaskItemModel::kTaskStateRole).toInt();
    can_cancel = state_value == static_cast<int>(pdftools_gui::services::TaskState::kQueued);
  }
  cancel_task_button_->setEnabled(can_cancel);
}

QString TaskCenterWidget::ResolveReportDirectory() const {
  if (!report_directory_.isEmpty()) {
    return report_directory_;
  }

  QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (base.isEmpty()) {
    base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  }
  if (base.isEmpty()) {
    base = QDir::tempPath();
  }
  return QDir(base).filePath(QStringLiteral("task_reports"));
}

}  // namespace pdftools_gui::widgets
