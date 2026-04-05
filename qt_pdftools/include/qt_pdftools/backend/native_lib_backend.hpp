#pragma once

#include "qt_pdftools/backend/task_backend.hpp"

namespace qt_pdftools::backend {

class NativeLibBackend final : public ITaskBackend {
 public:
  QString id() const override;
  QString label() const override;

  qt_pdftools::core::BackendStatus GetStatus() override;
  qt_pdftools::core::TaskResult RunTask(const qt_pdftools::core::TaskRequest& request) override;
};

}  // namespace qt_pdftools::backend
