#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <podofo/podofo.h>

#include "pdftools/pdf/create_ops.hpp"

int main() {
  namespace fs = std::filesystem;
  const fs::path out_dir = fs::temp_directory_path() / "pdftools_m4_test";
  fs::create_directories(out_dir);

  const fs::path image_path = out_dir / "tiny.png";
  const fs::path output_pdf = out_dir / "images.pdf";

  const std::array<unsigned char, 67> kTinyPng = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48,
      0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
      0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78,
      0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
      0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

  {
    std::ofstream image_out(image_path, std::ios::binary);
    image_out.write(reinterpret_cast<const char*>(kTinyPng.data()), static_cast<std::streamsize>(kTinyPng.size()));
  }
  if (!fs::exists(image_path) || fs::file_size(image_path) == 0) {
    return EXIT_FAILURE;
  }

  pdftools::pdf::ImagesToPdfRequest request;
  request.image_paths = {image_path.string(), image_path.string()};
  request.output_pdf = output_pdf.string();
  request.use_image_size_as_page = true;
  request.overwrite = true;

  pdftools::pdf::ImagesToPdfResult result;
  pdftools::Status status = pdftools::pdf::ImagesToPdf(request, &result);
  if (!status.ok()) {
    return EXIT_FAILURE;
  }
  if (result.page_count != 2) {
    return EXIT_FAILURE;
  }
  if (!fs::exists(output_pdf) || fs::file_size(output_pdf) == 0) {
    return EXIT_FAILURE;
  }

  PoDoFo::PdfMemDocument out_doc;
  out_doc.Load(output_pdf.string());
  if (out_doc.GetPages().GetCount() != 2) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

