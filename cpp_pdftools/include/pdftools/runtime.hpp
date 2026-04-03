#pragma once

#include <any>
#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>

#include "pdftools/status.hpp"

namespace pdftools {

class ILogger {
 public:
  virtual ~ILogger() = default;
  virtual void Debug(const std::string& message) = 0;
  virtual void Info(const std::string& message) = 0;
  virtual void Warn(const std::string& message) = 0;
  virtual void Error(const std::string& message) = 0;
};

class IProgressSink {
 public:
  virtual ~IProgressSink() = default;
  virtual void OnProgress(double value, const std::string& stage, const std::string& detail) = 0;
};

struct RuntimeContext {
  ILogger* logger = nullptr;
  IProgressSink* progress = nullptr;
  std::shared_ptr<std::atomic_bool> cancel_flag;

  bool IsCancellationRequested() const {
    return cancel_flag != nullptr && cancel_flag->load();
  }
};

class ICommandHandler {
 public:
  virtual ~ICommandHandler() = default;
  virtual Status Handle(const std::any& request, std::any* result, const RuntimeContext& context) = 0;
};

class CommandRegistry {
 public:
  Status Register(const std::string& operation_id, std::unique_ptr<ICommandHandler> handler);
  Status Execute(const std::string& operation_id,
                 const std::any& request,
                 std::any* result,
                 const RuntimeContext& context) const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::unique_ptr<ICommandHandler>> handlers_;
};

}  // namespace pdftools
