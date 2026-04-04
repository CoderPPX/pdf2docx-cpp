#include <any>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <stdexcept>

#include "pdftools/error_handling.hpp"
#include "pdftools/runtime.hpp"

namespace {

class ThrowingHandler final : public pdftools::ICommandHandler {
 public:
  pdftools::Status Handle(const std::any&, std::any*, const pdftools::RuntimeContext&) override {
    throw std::runtime_error("handler throw");
  }
};

}  // namespace

int main() {
  auto ok_status = pdftools::GuardStatus(
      []() -> pdftools::Status { return pdftools::Status::Ok(); },
      pdftools::ErrorCode::kInternalError,
      "should not fail",
      "guard.ok");
  if (!ok_status.ok()) {
    return EXIT_FAILURE;
  }

  auto runtime_error_status = pdftools::GuardStatus(
      []() -> pdftools::Status { throw std::runtime_error("boom"); },
      pdftools::ErrorCode::kPdfParseFailed,
      "unexpected failure",
      "guard.runtime_error");
  if (runtime_error_status.ok()) {
    return EXIT_FAILURE;
  }
  if (runtime_error_status.code() != pdftools::ErrorCode::kPdfParseFailed) {
    return EXIT_FAILURE;
  }
  if (runtime_error_status.message().find("boom") == std::string::npos) {
    return EXIT_FAILURE;
  }
  if (runtime_error_status.context() != "guard.runtime_error") {
    return EXIT_FAILURE;
  }

  auto bad_alloc_status = pdftools::GuardStatus(
      []() -> pdftools::Status { throw std::bad_alloc(); },
      pdftools::ErrorCode::kPdfParseFailed,
      "unexpected failure",
      "guard.bad_alloc");
  if (bad_alloc_status.ok()) {
    return EXIT_FAILURE;
  }
  if (bad_alloc_status.code() != pdftools::ErrorCode::kInternalError) {
    return EXIT_FAILURE;
  }
  if (bad_alloc_status.message().find("out of memory") == std::string::npos) {
    return EXIT_FAILURE;
  }

  if (std::string(pdftools::ErrorCodeToString(pdftools::ErrorCode::kIoError)) != "IO_ERROR") {
    return EXIT_FAILURE;
  }

  pdftools::CommandRegistry registry;
  auto register_status = registry.Register("throw.handler", std::make_unique<ThrowingHandler>());
  if (!register_status.ok()) {
    return EXIT_FAILURE;
  }

  std::any result;
  auto execute_status = registry.Execute("throw.handler", std::any{}, &result, {});
  if (execute_status.ok()) {
    return EXIT_FAILURE;
  }
  if (execute_status.code() != pdftools::ErrorCode::kInternalError) {
    return EXIT_FAILURE;
  }
  if (execute_status.context() != "throw.handler") {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
