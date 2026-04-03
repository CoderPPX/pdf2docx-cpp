#include "pdftools/convert/pdf2docx.hpp"

#include <filesystem>

#include "pdf2docx/converter.hpp"
#include "pdf2docx/ir_json.hpp"

namespace pdftools::convert {

namespace {

ErrorCode MapLegacyErrorCode(pdf2docx::ErrorCode code) {
  switch (code) {
    case pdf2docx::ErrorCode::kOk:
      return ErrorCode::kOk;
    case pdf2docx::ErrorCode::kInvalidArgument:
      return ErrorCode::kInvalidArgument;
    case pdf2docx::ErrorCode::kIoError:
      return ErrorCode::kIoError;
    case pdf2docx::ErrorCode::kPdfParseFailed:
      return ErrorCode::kPdfParseFailed;
    case pdf2docx::ErrorCode::kUnsupportedFeature:
      return ErrorCode::kUnsupportedFeature;
    case pdf2docx::ErrorCode::kInternalError:
      return ErrorCode::kInternalError;
  }
  return ErrorCode::kInternalError;
}

Status MapLegacyStatus(const pdf2docx::Status& status) {
  if (status.ok()) {
    return Status::Ok();
  }
  return Status::Error(MapLegacyErrorCode(status.code()), status.message());
}

Status PrepareOutputPath(const std::string& output_path, bool overwrite) {
  if (output_path.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output path must not be empty");
  }

  std::filesystem::path out(output_path);
  std::error_code ec;
  if (std::filesystem::exists(out, ec) && !overwrite) {
    return Status::Error(ErrorCode::kAlreadyExists, "output path already exists", output_path);
  }
  if (!out.parent_path().empty()) {
    std::filesystem::create_directories(out.parent_path(), ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError, "failed to create output directory", out.parent_path().string());
    }
  }
  return Status::Ok();
}

}  // namespace

Status ConvertPdfToDocx(const PdfToDocxRequest& request, PdfToDocxResult* result) {
  if (request.input_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input path must not be empty");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  Status output_status = PrepareOutputPath(request.output_docx, request.overwrite);
  if (!output_status.ok()) {
    return output_status;
  }
  if (!request.dump_ir_json_path.empty()) {
    output_status = PrepareOutputPath(request.dump_ir_json_path, request.overwrite);
    if (!output_status.ok()) {
      return output_status;
    }
  }

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  options.extract_images = request.extract_images;
  options.enable_font_fallback = request.enable_font_fallback;
  options.docx_use_anchored_images = request.use_anchored_images;

  pdf2docx::ConvertStats stats;
  pdf2docx::ir::Document ir_doc;
  pdf2docx::ir::Document* maybe_ir = request.dump_ir_json_path.empty() ? nullptr : &ir_doc;

  pdf2docx::Status convert_status = converter.ConvertFile(request.input_pdf, request.output_docx, options, &stats, maybe_ir);
  Status mapped_status = MapLegacyStatus(convert_status);
  if (!mapped_status.ok()) {
    return mapped_status;
  }

  if (!request.dump_ir_json_path.empty()) {
    pdf2docx::Status dump_status = pdf2docx::WriteIrToJson(ir_doc, request.dump_ir_json_path);
    mapped_status = MapLegacyStatus(dump_status);
    if (!mapped_status.ok()) {
      return mapped_status;
    }
  }

  result->page_count = stats.page_count;
  result->image_count = stats.image_count;
  result->warning_count = stats.warning_count;
  result->extracted_image_count = stats.extracted_image_count;
  result->skipped_image_count = stats.skipped_image_count;
  result->elapsed_ms = stats.elapsed_ms;
  result->backend = stats.backend;

  return Status::Ok();
}

}  // namespace pdftools::convert

