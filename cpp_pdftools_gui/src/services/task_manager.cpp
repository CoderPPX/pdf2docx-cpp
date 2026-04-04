#include "pdftools_gui/services/task_manager.hpp"

#include <algorithm>

#include <QtConcurrent>

namespace pdftools_gui::services {

namespace {

bool IsFinalState(TaskState state) {
  return state == TaskState::kSucceeded || state == TaskState::kFailed || state == TaskState::kCanceled;
}

}  // namespace

TaskManager::TaskManager(std::shared_ptr<PdfToolsService> service,
                         QObject* parent,
                         int max_completed_tasks,
                         int max_concurrent_tasks)
    : QObject(parent),
      service_(std::move(service)),
      max_completed_tasks_(std::max(1, max_completed_tasks)) {
  if (service_ == nullptr) {
    service_ = std::make_shared<PdfToolsService>();
  }
  thread_pool_.setMaxThreadCount(std::max(1, max_concurrent_tasks));
  qRegisterMetaType<pdftools_gui::services::TaskInfo>("pdftools_gui::services::TaskInfo");
}

qint64 TaskManager::Submit(OperationKind operation, const QString& display_name, RequestVariant request) {
  const qint64 task_id = next_id_.fetch_add(1);

  auto record = std::make_shared<TaskRecord>();
  record->info.id = task_id;
  record->info.operation = operation;
  record->info.display_name = display_name;
  record->info.state = TaskState::kQueued;
  record->info.progress = 0;
  record->info.submitted_at = QDateTime::currentDateTime();
  record->watcher = new QFutureWatcher<ExecutionOutcome>(this);

  {
    QMutexLocker lock(&mutex_);
    tasks_.insert(task_id, record);
  }

  emit TaskChanged(record->info);

  connect(record->watcher, &QFutureWatcher<ExecutionOutcome>::started, this, [this, task_id]() {
    std::shared_ptr<TaskRecord> started_record;
    {
      QMutexLocker lock(&mutex_);
      auto it = tasks_.find(task_id);
      if (it == tasks_.end()) {
        return;
      }
      started_record = it.value();
      started_record->info.state = TaskState::kRunning;
      started_record->info.progress = 5;
    }
    emit TaskChanged(started_record->info);
  });

  connect(record->watcher, &QFutureWatcher<ExecutionOutcome>::finished, this, [this, task_id]() { FinishTask(task_id); });

  QFuture<ExecutionOutcome> future =
      QtConcurrent::run(&thread_pool_, [service = service_,
                                        operation,
                                        request = std::move(request),
                                        cancel_requested = record->cancel_requested]() {
        if (cancel_requested->load()) {
          ExecutionOutcome outcome;
          outcome.success = false;
          outcome.summary = QStringLiteral("Canceled");
          outcome.detail = QStringLiteral("Task was canceled before execution started.");
          return outcome;
        }
        return service->Execute(operation, request);
      });
  record->watcher->setFuture(future);

  return task_id;
}

bool TaskManager::CancelTask(qint64 task_id) {
  std::shared_ptr<TaskRecord> record;
  QFutureWatcher<ExecutionOutcome>* watcher_to_delete = nullptr;
  {
    QMutexLocker lock(&mutex_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
      return false;
    }
    record = it.value();
    if (record->terminal_emitted || record->info.state != TaskState::kQueued) {
      return false;
    }
    record->cancel_requested->store(true);
    record->terminal_emitted = true;
    record->info.state = TaskState::kCanceled;
    record->info.progress = 100;
    record->info.finished_at = QDateTime::currentDateTime();
    record->info.summary = QStringLiteral("Canceled");
    record->info.detail = QStringLiteral("Task canceled from queue before execution.");
    watcher_to_delete = record->watcher;
    record->watcher = nullptr;
    completed_order_.enqueue(task_id);
    while (completed_order_.size() > max_completed_tasks_) {
      const qint64 evicted_task_id = completed_order_.dequeue();
      tasks_.remove(evicted_task_id);
    }
  }

  emit TaskChanged(record->info);
  emit TaskCompleted(record->info);

  if (watcher_to_delete != nullptr) {
    watcher_to_delete->deleteLater();
  }
  return true;
}

void TaskManager::ClearRetainedTasks() {
  QMutexLocker lock(&mutex_);
  completed_order_.clear();

  QList<qint64> remove_task_ids;
  for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
    const auto& record = it.value();
    if (record == nullptr || !IsFinalState(record->info.state)) {
      continue;
    }
    if (record->watcher != nullptr) {
      record->watcher->deleteLater();
      record->watcher = nullptr;
    }
    remove_task_ids.push_back(it.key());
  }

  for (const qint64 id : remove_task_ids) {
    tasks_.remove(id);
  }
}

void TaskManager::SetMaxCompletedTasks(int max_completed_tasks) {
  const int normalized = std::max(1, max_completed_tasks);
  QMutexLocker lock(&mutex_);
  max_completed_tasks_ = normalized;
  while (completed_order_.size() > max_completed_tasks_) {
    const qint64 evicted_task_id = completed_order_.dequeue();
    tasks_.remove(evicted_task_id);
  }
}

int TaskManager::MaxCompletedTasks() const {
  QMutexLocker lock(&mutex_);
  return max_completed_tasks_;
}

void TaskManager::SetMaxConcurrentTasks(int max_concurrent_tasks) {
  thread_pool_.setMaxThreadCount(std::max(1, max_concurrent_tasks));
}

int TaskManager::MaxConcurrentTasks() const {
  return thread_pool_.maxThreadCount();
}

int TaskManager::RetainedTaskCount() const {
  QMutexLocker lock(&mutex_);
  return tasks_.size();
}

void TaskManager::FinishTask(qint64 task_id) {
  std::shared_ptr<TaskRecord> record;
  {
    QMutexLocker lock(&mutex_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
      return;
    }
    record = it.value();
    if (record->terminal_emitted) {
      if (record->watcher != nullptr) {
        record->watcher->deleteLater();
        record->watcher = nullptr;
      }
      return;
    }
    record->terminal_emitted = true;
  }

  if (record->watcher == nullptr) {
    return;
  }
  const ExecutionOutcome outcome = record->watcher->result();
  record->info.finished_at = QDateTime::currentDateTime();
  record->info.progress = 100;
  record->info.summary = outcome.summary;
  record->info.detail = outcome.detail;
  record->info.output_path = outcome.output_path;
  record->info.state = outcome.success ? TaskState::kSucceeded : TaskState::kFailed;

  emit TaskChanged(record->info);
  emit TaskCompleted(record->info);

  {
    QMutexLocker lock(&mutex_);
    completed_order_.enqueue(task_id);
    while (completed_order_.size() > max_completed_tasks_) {
      const qint64 evicted_task_id = completed_order_.dequeue();
      tasks_.remove(evicted_task_id);
    }
  }

  record->watcher->deleteLater();
  record->watcher = nullptr;
}

QString TaskStateToDisplayString(TaskState state) {
  switch (state) {
    case TaskState::kQueued:
      return QStringLiteral("Queued");
    case TaskState::kRunning:
      return QStringLiteral("Running");
    case TaskState::kSucceeded:
      return QStringLiteral("Succeeded");
    case TaskState::kFailed:
      return QStringLiteral("Failed");
    case TaskState::kCanceled:
      return QStringLiteral("Canceled");
  }
  return QStringLiteral("Unknown");
}

}  // namespace pdftools_gui::services
