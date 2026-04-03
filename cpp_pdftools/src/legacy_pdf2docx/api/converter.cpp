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
                                    ir::Document* document,
                                    ImageExtractionStats* image_stats) const {
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
  return backend_impl.ExtractToIr(input_pdf, options, document, image_stats);
}

Status Converter::ConvertFile(const std::string& input_pdf,
                              const std::string& output_docx,
                              const ConvertOptions& options,
                              ConvertStats* stats) const {
  return ConvertFile(input_pdf, output_docx, options, stats, nullptr);
}

Status Converter::ConvertFile(const std::string& input_pdf,
                              const std::string& output_docx,
                              const ConvertOptions& options,
                              ConvertStats* stats,
                              ir::Document* out_document) const {
  if (input_pdf.empty() || output_docx.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input/output path must not be empty");
  }

  const auto started = std::chrono::steady_clock::now();

  ir::Document local_document;
  ir::Document* ir_document = out_document != nullptr ? out_document : &local_document;
  ImageExtractionStats image_extraction_stats;
  const auto extract_started = std::chrono::steady_clock::now();
  Status extract_status = ExtractIrFromFile(input_pdf, options, ir_document, &image_extraction_stats);
  const auto extract_ended = std::chrono::steady_clock::now();
  if (!extract_status.ok()) {
    return extract_status;
  }

  pipeline::Pipeline pipeline;
  pipeline::PipelineStats pipeline_stats;
  const auto pipeline_started = std::chrono::steady_clock::now();
  Status pipeline_status = pipeline.Execute(ir_document, &pipeline_stats);
  const auto pipeline_ended = std::chrono::steady_clock::now();
  if (!pipeline_status.ok()) {
    return pipeline_status;
  }

  ConvertStats local_stats;
  local_stats.page_count = static_cast<uint32_t>(ir_document->pages.size());
  local_stats.image_count = CountImages(*ir_document);
  local_stats.warning_count = image_extraction_stats.warning_count;
  local_stats.extracted_image_count = image_extraction_stats.extracted_count;
  local_stats.skipped_image_count = image_extraction_stats.skipped_count;
  local_stats.skipped_unsupported_filter_count = image_extraction_stats.skipped_unsupported_filter_count;
  local_stats.skipped_empty_stream_count = image_extraction_stats.skipped_empty_stream_count;
  local_stats.skipped_decode_failed_count = image_extraction_stats.skipped_decode_failed_count;
  local_stats.font_probe_count = options.enable_font_fallback ? 1u : 0u;
  local_stats.backend_warning_count = image_extraction_stats.warning_count;
  local_stats.backend = BackendToString(backend_);
  local_stats.xml_backend = PDF2DOCX_XML_BACKEND_STR;
  local_stats.extract_elapsed_ms =
      static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(extract_ended - extract_started)
                              .count());
  local_stats.pipeline_elapsed_ms =
      static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(pipeline_ended - pipeline_started)
                              .count());

  docx::P0Writer writer;
  docx::DocxWriteOptions write_options;
  write_options.use_anchored_images = options.docx_use_anchored_images;
  const auto write_started = std::chrono::steady_clock::now();
  Status write_status = writer.WriteFromIr(*ir_document, output_docx, local_stats, write_options);
  const auto write_ended = std::chrono::steady_clock::now();
  if (!write_status.ok()) {
    return write_status;
  }
  local_stats.write_elapsed_ms =
      static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(write_ended - write_started)
                              .count());

  const auto ended = write_ended;
  local_stats.elapsed_ms =
      static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count());

  if (stats != nullptr) {
    *stats = local_stats;
  }

  return Status::Ok();
}

}  // namespace pdf2docx
