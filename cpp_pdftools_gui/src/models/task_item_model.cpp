#include "pdftools_gui/models/task_item_model.hpp"

#include <utility>

#include "pdftools_gui/services/pdftools_service.hpp"

namespace pdftools_gui::models {

namespace {

bool IsFinalState(pdftools_gui::services::TaskState state) {
  return state == pdftools_gui::services::TaskState::kSucceeded ||
         state == pdftools_gui::services::TaskState::kFailed ||
         state == pdftools_gui::services::TaskState::kCanceled;
}

}  // namespace

TaskItemModel::TaskItemModel(QObject* parent) : QAbstractTableModel(parent) {}

int TaskItemModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return tasks_.size();
}

int TaskItemModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return kColumnCount;
}

QVariant TaskItemModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= tasks_.size()) {
    return {};
  }

  const auto& task = tasks_.at(index.row());

  if (role == kTaskIdRole) {
    return task.id;
  }
  if (role == kTaskStateRole) {
    return static_cast<int>(task.state);
  }
  if (role == kOutputPathRole) {
    return task.output_path;
  }
  if (role == kDetailRole) {
    return task.detail;
  }

  if (role != Qt::DisplayRole) {
    return {};
  }

  switch (index.column()) {
    case kId:
      return task.id;
    case kOperation:
      return services::PdfToolsService::OperationDisplayName(task.operation);
    case kStatus:
      return services::TaskStateToDisplayString(task.state);
    case kProgress:
      return QStringLiteral("%1%").arg(task.progress);
    case kSubmittedAt:
      return task.submitted_at.isValid() ? task.submitted_at.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    case kFinishedAt:
      return task.finished_at.isValid() ? task.finished_at.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    case kSummary:
      return task.summary;
    default:
      return {};
  }
}

QVariant TaskItemModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return QAbstractTableModel::headerData(section, orientation, role);
  }

  switch (section) {
    case kId:
      return QStringLiteral("ID");
    case kOperation:
      return QStringLiteral("Operation");
    case kStatus:
      return QStringLiteral("Status");
    case kProgress:
      return QStringLiteral("Progress");
    case kSubmittedAt:
      return QStringLiteral("Submitted");
    case kFinishedAt:
      return QStringLiteral("Finished");
    case kSummary:
      return QStringLiteral("Summary");
    default:
      return {};
  }
}

void TaskItemModel::UpsertTask(const pdftools_gui::services::TaskInfo& info) {
  const int existing_row = FindRowByTaskId(info.id);
  if (existing_row < 0) {
    beginInsertRows(QModelIndex(), 0, 0);
    tasks_.prepend(info);
    endInsertRows();
    return;
  }

  tasks_[existing_row] = info;
  const QModelIndex left = index(existing_row, 0);
  const QModelIndex right = index(existing_row, kColumnCount - 1);
  emit dataChanged(left, right);
}

QVector<pdftools_gui::services::TaskInfo> TaskItemModel::SnapshotTasks() const {
  return tasks_;
}

void TaskItemModel::ReplaceAllTasks(const QVector<pdftools_gui::services::TaskInfo>& tasks) {
  beginResetModel();
  tasks_ = tasks;
  endResetModel();
}

void TaskItemModel::RemoveFinalTasks() {
  QVector<pdftools_gui::services::TaskInfo> active_tasks;
  active_tasks.reserve(tasks_.size());
  for (const auto& task : tasks_) {
    if (!IsFinalState(task.state)) {
      active_tasks.push_back(task);
    }
  }

  if (active_tasks.size() == tasks_.size()) {
    return;
  }

  beginResetModel();
  tasks_ = std::move(active_tasks);
  endResetModel();
}

int TaskItemModel::FindRowByTaskId(qint64 task_id) const {
  for (int i = 0; i < tasks_.size(); ++i) {
    if (tasks_.at(i).id == task_id) {
      return i;
    }
  }
  return -1;
}

}  // namespace pdftools_gui::models
