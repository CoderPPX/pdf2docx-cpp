#include "pdftools/runtime.hpp"

namespace pdftools {

Status CommandRegistry::Register(const std::string& operation_id, std::unique_ptr<ICommandHandler> handler) {
  if (operation_id.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "operation_id must not be empty");
  }
  if (handler == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "handler is null");
  }

  std::scoped_lock lock(mutex_);
  if (handlers_.find(operation_id) != handlers_.end()) {
    return Status::Error(ErrorCode::kAlreadyExists, "operation already registered", operation_id);
  }

  handlers_.emplace(operation_id, std::move(handler));
  return Status::Ok();
}

Status CommandRegistry::Execute(const std::string& operation_id,
                                const std::any& request,
                                std::any* result,
                                const RuntimeContext& context) const {
  ICommandHandler* handler = nullptr;
  {
    std::scoped_lock lock(mutex_);
    auto it = handlers_.find(operation_id);
    if (it == handlers_.end()) {
      return Status::Error(ErrorCode::kNotFound, "operation not found", operation_id);
    }
    handler = it->second.get();
  }

  if (context.IsCancellationRequested()) {
    return Status::Error(ErrorCode::kCancelled, "operation cancelled before start", operation_id);
  }

  return handler->Handle(request, result, context);
}

}  // namespace pdftools

