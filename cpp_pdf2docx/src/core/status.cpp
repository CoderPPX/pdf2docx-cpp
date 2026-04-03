#include "pdf2docx/status.hpp"

#include <utility>

namespace pdf2docx {

Status::Status(ErrorCode code, std::string message)
    : code_(code), message_(std::move(message)) {}

Status Status::Ok() {
  return Status(ErrorCode::kOk, "");
}

Status Status::Error(ErrorCode code, std::string message) {
  return Status(code, std::move(message));
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

}  // namespace pdf2docx
