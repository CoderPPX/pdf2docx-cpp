#pragma once

#include <atomic>
#include <memory>
#include <variant>

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThreadPool>

#include "pdftools_gui/services/pdftools_service.hpp"

namespace pdftools_gui::services {

enum class TaskState {
  kQueued = 0,
  kRunning,
  kSucceeded,
  kFailed,
  kCanceled,
};

struct TaskInfo {
  qint64 id = 0;
  OperationKind operation = OperationKind::kMergePdf;
  QString display_name;
  TaskState state = TaskState::kQueued;
  int progress = 0;
  QDateTime submitted_at;
  QDateTime finished_at;
  QString summary;
  QString detail;
  QString output_path;
};

class TaskManager : public QObject {
  Q_OBJECT

 public:
  explicit TaskManager(std::shared_ptr<PdfToolsService> service,
                       QObject* parent = nullptr,
                       int max_completed_tasks = 200,
                       int max_concurrent_tasks = 2);

  qint64 Submit(OperationKind operation, const QString& display_name, RequestVariant request);
  bool CancelTask(qint64 task_id);
  void ClearRetainedTasks();
  void SetMaxCompletedTasks(int max_completed_tasks);
  int MaxCompletedTasks() const;
  void SetMaxConcurrentTasks(int max_concurrent_tasks);
  int MaxConcurrentTasks() const;
  int RetainedTaskCount() const;

 signals:
  void TaskChanged(const pdftools_gui::services::TaskInfo& info);
  void TaskCompleted(const pdftools_gui::services::TaskInfo& info);

 private:
  struct TaskRecord {
    TaskInfo info;
    QFutureWatcher<ExecutionOutcome>* watcher = nullptr;
    std::shared_ptr<std::atomic_bool> cancel_requested = std::make_shared<std::atomic_bool>(false);
    bool terminal_emitted = false;
  };

  void FinishTask(qint64 task_id);

  std::shared_ptr<PdfToolsService> service_;
  int max_completed_tasks_ = 200;
  mutable QMutex mutex_;
  QHash<qint64, std::shared_ptr<TaskRecord>> tasks_;
  QQueue<qint64> completed_order_;
  QThreadPool thread_pool_;
  std::atomic<qint64> next_id_{1};
};

QString TaskStateToDisplayString(TaskState state);

}  // namespace pdftools_gui::services

Q_DECLARE_METATYPE(pdftools_gui::services::TaskInfo)
