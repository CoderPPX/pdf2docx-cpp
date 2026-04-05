#include "qt_pdftools/backend/backend_registry.hpp"

namespace qt_pdftools::backend {

BackendRegistry::BackendRegistry()
    : native_lib_(std::make_unique<NativeLibBackend>()) {}

ITaskBackend* BackendRegistry::Get(const QString& id) const {
  if (native_lib_ != nullptr && id == native_lib_->id()) {
    return native_lib_.get();
  }
  return nullptr;
}

QStringList BackendRegistry::BackendIds() const {
  QStringList ids;
  if (native_lib_ != nullptr) {
    ids << native_lib_->id();
  }
  return ids;
}

}  // namespace qt_pdftools::backend
