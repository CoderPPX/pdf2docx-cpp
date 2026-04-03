#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "backend/podofo/podofo_backend.hpp"
#include "pdf2docx/status.hpp"

namespace {

bool ExpectCode(const pdf2docx::Status& status, pdf2docx::ErrorCode code) {
  return status.code() == code;
}

}  // namespace

int main() {
  using pdf2docx::ErrorCode;
  using pdf2docx::backend::PoDoFoBackend;

  PoDoFoBackend backend;

  if (!ExpectCode(backend.Open(""), ErrorCode::kInvalidArgument)) {
    return EXIT_FAILURE;
  }

  if (!ExpectCode(backend.Open("definitely-not-existing.pdf"), ErrorCode::kIoError)) {
    return EXIT_FAILURE;
  }

  const std::filesystem::path tmp_dir = std::filesystem::temp_directory_path() / "pdf2docx_backend_test";
  std::filesystem::create_directories(tmp_dir);

  const std::filesystem::path txt_path = tmp_dir / "sample.txt";
  {
    std::ofstream txt_stream(txt_path, std::ios::binary | std::ios::trunc);
    txt_stream << "hello";
  }
  if (!ExpectCode(backend.Open(txt_path.string()), ErrorCode::kInvalidArgument)) {
    return EXIT_FAILURE;
  }

  const std::filesystem::path pdf_path = tmp_dir / "sample.pdf";
  {
    std::ofstream pdf_stream(pdf_path, std::ios::binary | std::ios::trunc);
    pdf_stream << "%PDF-1.4\n";
    pdf_stream << "1 0 obj\n<<>>\nendobj\n";
    pdf_stream << "trailer\n<<>>\n%%EOF\n";
  }

  if (!backend.Open(pdf_path.string()).ok()) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
