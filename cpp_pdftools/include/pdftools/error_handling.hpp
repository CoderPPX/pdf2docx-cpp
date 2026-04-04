#pragma once

#include <exception>
#include <string_view>
#include <type_traits>
#include <utility>

#include "pdftools/status.hpp"

namespace pdftools {

const char* ErrorCodeToString(ErrorCode code);

Status ExceptionToStatus(const std::exception& exception,
                         ErrorCode fallback_code,
                         std::string_view context = {});

Status CurrentExceptionToStatus(ErrorCode fallback_code,
                                std::string_view fallback_message,
                                std::string_view context = {});

template <typename Fn>
Status GuardStatus(Fn&& fn,
                   ErrorCode fallback_code,
                   std::string_view fallback_message,
                   std::string_view context = {}) {
  static_assert(std::is_invocable_v<Fn>, "GuardStatus expects an invocable callable");
  static_assert(std::is_convertible_v<std::invoke_result_t<Fn>, Status>,
                "GuardStatus expects callable result convertible to pdftools::Status");
  try {
    return static_cast<Status>(std::forward<Fn>(fn)());
  } catch (...) {
    return CurrentExceptionToStatus(fallback_code, fallback_message, context);
  }
}

}  // namespace pdftools
