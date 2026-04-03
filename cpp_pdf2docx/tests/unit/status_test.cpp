#include <cstdlib>

#include "pdf2docx/status.hpp"

int main() {
  pdf2docx::Status ok = pdf2docx::Status::Ok();
  if (!ok.ok()) {
    return EXIT_FAILURE;
  }

  pdf2docx::Status error = pdf2docx::Status::Error(pdf2docx::ErrorCode::kInternalError, "x");
  if (error.ok()) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
