#pragma once

#include <memory>

#include "qt_pdftools/backend/native_lib_backend.hpp"

namespace qt_pdftools::backend {

class BackendRegistry {
 public:
  BackendRegistry();

  ITaskBackend* Get(const QString& id) const;
  QStringList BackendIds() const;

 private:
  std::unique_ptr<NativeLibBackend> native_lib_;
};

}  // namespace qt_pdftools::backend
