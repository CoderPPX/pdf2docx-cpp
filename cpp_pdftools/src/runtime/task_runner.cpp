#include "pdftools/task.hpp"

#include <thread>

namespace pdftools {

struct TaskRunner::TaskRecord {
  TaskState state;
  std::shared_ptr<std::atomic_bool> cancel_flag;
};

namespace {

class CallbackProgressSink final : public IProgressSink {
 public:
  CallbackProgressSink(TaskHandle handle, ProgressCallback callback) : handle_(handle), callback_(std::move(callback)) {}

  void OnProgress(double value, const std::string& stage, const std::string& detail) override {
    if (!callback_) {
      return;
    }
    TaskState state;
    state.status = TaskStatus::kRunning;
    state.progress = value;
    state.message = stage + (detail.empty() ? "" : (": " + detail));
    callback_(handle_, state);
  }

 private:
  TaskHandle handle_ = 0;
  ProgressCallback callback_;
};

}  // namespace

TaskRunner::TaskRunner(const CommandRegistry& registry) : registry_(registry) {}

TaskHandle TaskRunner::Submit(const TaskRequest& request,
                              const RuntimeContext& base_context,
                              ProgressCallback on_progress,
                              FinishCallback on_finish) {
  TaskHandle handle = next_id_.fetch_add(1);
  auto record = std::make_shared<TaskRecord>();
  record->state.status = TaskStatus::kQueued;
  record->state.progress = 0.0;
  record->state.message = "queued";
  record->cancel_flag = std::make_shared<std::atomic_bool>(false);

  {
    std::scoped_lock lock(mutex_);
    tasks_[handle] = record;
  }

  std::thread([this, handle, request, base_context, on_progress = std::move(on_progress), on_finish = std::move(on_finish),
               record]() mutable {
    RuntimeContext context = base_context;
    context.cancel_flag = record->cancel_flag;
    CallbackProgressSink callback_progress_sink(handle, on_progress);
    if (context.progress == nullptr && on_progress) {
      context.progress = &callback_progress_sink;
    }

    {
      std::scoped_lock lock(mutex_);
      if (record->cancel_flag->load()) {
        record->state.status = TaskStatus::kCancelled;
        record->state.message = "cancelled";
      } else {
        record->state.status = TaskStatus::kRunning;
        record->state.progress = 0.0;
        record->state.message = "running";
      }
    }

    if (record->cancel_flag->load()) {
      if (on_finish) {
        on_finish(handle, Status::Error(ErrorCode::kCancelled, "cancelled"), std::any{});
      }
      return;
    }

    std::any result;
    Status status = registry_.Execute(request.operation_id, request.payload, &result, context);

    {
      std::scoped_lock lock(mutex_);
      if (record->cancel_flag->load() || status.code() == ErrorCode::kCancelled) {
        record->state.status = TaskStatus::kCancelled;
        record->state.message = status.message().empty() ? "cancelled" : status.message();
      } else if (!status.ok()) {
        record->state.status = TaskStatus::kFailed;
        record->state.message = status.message();
      } else {
        record->state.status = TaskStatus::kSucceeded;
        record->state.progress = 1.0;
        record->state.message = "succeeded";
      }
    }

    if (on_finish) {
      on_finish(handle, status, result);
    }
  }).detach();

  return handle;
}

Status TaskRunner::Cancel(TaskHandle handle) {
  std::shared_ptr<TaskRecord> record;
  {
    std::scoped_lock lock(mutex_);
    auto it = tasks_.find(handle);
    if (it == tasks_.end()) {
      return Status::Error(ErrorCode::kNotFound, "task handle not found");
    }
    record = it->second;
    record->cancel_flag->store(true);
    if (record->state.status == TaskStatus::kQueued || record->state.status == TaskStatus::kRunning) {
      record->state.message = "cancellation requested";
    }
  }
  return Status::Ok();
}

Status TaskRunner::Query(TaskHandle handle, TaskState* state) const {
  if (state == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "state pointer is null");
  }

  std::scoped_lock lock(mutex_);
  auto it = tasks_.find(handle);
  if (it == tasks_.end()) {
    return Status::Error(ErrorCode::kNotFound, "task handle not found");
  }

  *state = it->second->state;
  return Status::Ok();
}

}  // namespace pdftools

