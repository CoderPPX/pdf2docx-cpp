#include "pdf2docx/converter.hpp"

#include <chrono>

#include "backend/podofo/podofo_backend.hpp"
#include "docx/p0_writer.hpp"
#include "pipeline/pipeline.hpp"
#include "pdf2docx/font.hpp"

namespace pdf2docx {

namespace {

std::string BackendToString(BackendKind backend) {
  switch (backend) {
    case BackendKind::kPoDoFo:
      return "podofo";
    case BackendKind::kPoppler:
      return "poppler";
  }
  return "unknown";
}

uint32_t CountImages(const ir::Document& document) {
  size_t total = 0;
  for (const auto& page : document.pages) {
    total += page.images.size();
  }
  return static_cast<uint32_t>(total);
}

}  // namespace

Converter::Converter(BackendKind backend) : backend_(backend) {}

Status Converter::ExtractIrFromFile(const std::string& input_pdf,
                                    const ConvertOptions& options,
                                    ir::Document* document) const {
  if (input_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input path must not be empty");
  }
  if (document == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "document pointer is null");
  }

  if (options.enable_font_fallback) {
    Status freetype_status = ProbeFreeType();
    if (!freetype_status.ok()) {
      return freetype_status;
    }
  }

  if (backend_ != BackendKind::kPoDoFo) {
    return Status::Error(ErrorCode::kUnsupportedFeature, "only PoDoFo backend is implemented in this stage");
  }

  backend::PoDoFoBackend backend_impl;
  return backend_impl.ExtractToIr(input_pdf, options, document);
}

Status Converter::ConvertFile(const std::string& input_pdf,
                              const std::string& output_docx,
                              const ConvertOptions& options,
                              ConvertStats* stats) const {
  if (input_pdf.empty() || output_docx.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input/output path must not be empty");
  }

  const auto started = std::chrono::steady_clock::now();

  ir::Document ir_document;
  Status extract_status = ExtractIrFromFile(input_pdf, options, &ir_document);
  if (!extract_status.ok()) {
    return extract_status;
  }

  pipeline::Pipeline pipeline;
  Status pipeline_status = pipeline.Execute();
  if (!pipeline_status.ok()) {
    return pipeline_status;
  }

  ConvertStats local_stats;
  local_stats.page_count = static_cast<uint32_t>(ir_document.pages.size());
  local_stats.image_count = CountImages(ir_document);
  local_stats.warning_count = 0;
  local_stats.backend = BackendToString(backend_);
  local_stats.xml_backend = PDF2DOCX_XML_BACKEND_STR;

  docx::P0Writer writer;
  Status write_status = writer.WriteFromIr(ir_document, output_docx, local_stats);
  if (!write_status.ok()) {
    return write_status;
  }

  const auto ended = std::chrono::steady_clock::now();
  local_stats.elapsed_ms =
      static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count());

  if (stats != nullptr) {
    *stats = local_stats;
  }

  return Status::Ok();
}

}  // namespace pdf2docx
