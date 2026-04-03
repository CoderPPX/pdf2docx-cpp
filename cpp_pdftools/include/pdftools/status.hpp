#pragma once

#include <string>

namespace pdftools {

enum class ErrorCode {
  kOk = 0,
  kInvalidArgument,
  kIoError,
  kPdfParseFailed,
  kUnsupportedFeature,
  kInternalError,
  kNotFound,
  kAlreadyExists,
  kCancelled,
};

class Status {
 public:
  Status();

  static Status Ok();
  static Status Error(ErrorCode code, std::string message, std::string context = {});

  bool ok() const;
  ErrorCode code() const;
  const std::string& message() const;
  const std::string& context() const;

 private:
  Status(ErrorCode code, std::string message, std::string context);

  ErrorCode code_;
  std::string message_;
  std::string context_;
};

}  // namespace pdftools

