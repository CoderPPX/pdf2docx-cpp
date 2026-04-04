#include "pdftools/error_handling.hpp"

#include <exception>
#include <new>
#include <string>

namespace pdftools {

const char* ErrorCodeToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::kOk:
      return "OK";
    case ErrorCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case ErrorCode::kIoError:
      return "IO_ERROR";
    case ErrorCode::kPdfParseFailed:
      return "PDF_PARSE_FAILED";
    case ErrorCode::kUnsupportedFeature:
      return "UNSUPPORTED_FEATURE";
    case ErrorCode::kInternalError:
      return "INTERNAL_ERROR";
    case ErrorCode::kNotFound:
      return "NOT_FOUND";
    case ErrorCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case ErrorCode::kCancelled:
      return "CANCELLED";
  }
  return "INTERNAL_ERROR";
}

Status ExceptionToStatus(const std::exception& exception,
                         ErrorCode fallback_code,
                         std::string_view context) {
  return Status::Error(fallback_code, exception.what(), std::string(context));
}

Status CurrentExceptionToStatus(ErrorCode fallback_code,
                                std::string_view fallback_message,
                                std::string_view context) {
  std::exception_ptr current = std::current_exception();
  if (!current) {
    return Status::Error(fallback_code, std::string(fallback_message), std::string(context));
  }

  try {
    std::rethrow_exception(current);
  } catch (const std::bad_alloc&) {
    return Status::Error(ErrorCode::kInternalError, "out of memory", std::string(context));
  } catch (const std::exception& exception) {
    return ExceptionToStatus(exception, fallback_code, context);
  } catch (...) {
    return Status::Error(fallback_code, std::string(fallback_message), std::string(context));
  }
}

}  // namespace pdftools
