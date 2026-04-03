#include "pdftools/status.hpp"

namespace pdftools {

Status::Status() : code_(ErrorCode::kOk), message_(), context_() {}

Status::Status(ErrorCode code, std::string message, std::string context)
    : code_(code), message_(std::move(message)), context_(std::move(context)) {}

Status Status::Ok() {
  return Status();
}

Status Status::Error(ErrorCode code, std::string message, std::string context) {
  return Status(code, std::move(message), std::move(context));
}

bool Status::ok() const {
  return code_ == ErrorCode::kOk;
}

ErrorCode Status::code() const {
  return code_;
}

const std::string& Status::message() const {
  return message_;
}

const std::string& Status::context() const {
  return context_;
}

}  // namespace pdftools

