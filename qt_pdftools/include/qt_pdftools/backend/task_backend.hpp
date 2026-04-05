#pragma once

#include <memory>

#include "qt_pdftools/core/task_types.hpp"

namespace qt_pdftools::backend {

class ITaskBackend {
 public:
  virtual ~ITaskBackend() = default;

  virtual QString id() const = 0;
  virtual QString label() const = 0;

  virtual qt_pdftools::core::BackendStatus GetStatus() = 0;
  virtual qt_pdftools::core::TaskResult RunTask(const qt_pdftools::core::TaskRequest& request) = 0;
};

using TaskBackendPtr = std::unique_ptr<ITaskBackend>;

}  // namespace qt_pdftools::backend
