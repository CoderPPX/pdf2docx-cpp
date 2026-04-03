#pragma once

#include <string>

namespace pdf2docx {

enum class ErrorCode {
  kOk = 0,
  kInvalidArgument,
  kIoError,
  kPdfParseFailed,
  kUnsupportedFeature,
  kInternalError,
};

class Status {
 public:
  static Status Ok();
  static Status Error(ErrorCode code, std::string message);

  bool ok() const;
  ErrorCode code() const;
  const std::string& message() const;

 private:
  Status(ErrorCode code, std::string message);

  ErrorCode code_;
  std::string message_;
};

}  // namespace pdf2docx
