#pragma once

#include <any>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include "pdftools/runtime.hpp"
#include "pdftools/status.hpp"

namespace pdftools {

using TaskHandle = uint64_t;

enum class TaskStatus {
  kUnknown = 0,
  kQueued,
  kRunning,
  kSucceeded,
  kFailed,
  kCancelled,
};

struct TaskState {
  TaskStatus status = TaskStatus::kUnknown;
  double progress = 0.0;
  std::string message;
};

struct TaskRequest {
  std::string operation_id;
  std::any payload;
};

using ProgressCallback = std::function<void(TaskHandle, const TaskState&)>;
using FinishCallback = std::function<void(TaskHandle, const Status&, const std::any&)>;

class TaskRunner {
 public:
  explicit TaskRunner(const CommandRegistry& registry);

  TaskHandle Submit(const TaskRequest& request,
                    const RuntimeContext& base_context = {},
                    ProgressCallback on_progress = nullptr,
                    FinishCallback on_finish = nullptr);

  Status Cancel(TaskHandle handle);
  Status Query(TaskHandle handle, TaskState* state) const;

 private:
  struct TaskRecord;

  const CommandRegistry& registry_;
  mutable std::mutex mutex_;
  std::unordered_map<TaskHandle, std::shared_ptr<TaskRecord>> tasks_;
  std::atomic<uint64_t> next_id_{1};
};

}  // namespace pdftools
