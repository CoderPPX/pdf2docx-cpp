#include "pdftools/pdf/document_ops.hpp"

#include <filesystem>
#include <string>

#include <podofo/podofo.h>

namespace pdftools::pdf {

namespace {

Status PrepareOutputPath(const std::string& output_path, bool overwrite) {
  if (output_path.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "output path must not be empty");
  }

  std::filesystem::path out(output_path);
  std::error_code ec;
  if (std::filesystem::exists(out, ec) && !overwrite) {
    return Status::Error(ErrorCode::kAlreadyExists, "output path already exists", output_path);
  }

  auto parent = out.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return Status::Error(ErrorCode::kIoError, "failed to create output directory", parent.string());
    }
  }

  return Status::Ok();
}

Status ValidatePageNumber(uint32_t page_number, uint32_t page_count, const std::string& field_name) {
  if (page_number == 0 || page_number > page_count) {
    return Status::Error(ErrorCode::kInvalidArgument, "page number out of range", field_name);
  }
  return Status::Ok();
}

}  // namespace

Status MergePdf(const MergePdfRequest& request, MergePdfResult* result) {
  if (request.input_pdfs.size() < 2) {
    return Status::Error(ErrorCode::kInvalidArgument, "at least two input PDFs are required");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  Status output_status = PrepareOutputPath(request.output_pdf, request.overwrite);
  if (!output_status.ok()) {
    return output_status;
  }

  try {
    PoDoFo::PdfMemDocument output_document;
    for (const auto& input_path : request.input_pdfs) {
      if (input_path.empty()) {
        return Status::Error(ErrorCode::kInvalidArgument, "input PDF path must not be empty");
      }

      PoDoFo::PdfMemDocument input_document;
      input_document.Load(input_path);
      output_document.GetPages().AppendDocumentPages(input_document);
    }

    output_document.Save(request.output_pdf);
    result->output_page_count = output_document.GetPages().GetCount();
    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kPdfParseFailed, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "failed to merge PDF documents");
  }
}

Status GetPdfInfo(const PdfInfoRequest& request, PdfInfoResult* result) {
  if (request.input_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input path must not be empty");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  std::error_code ec;
  if (!std::filesystem::exists(request.input_pdf, ec) || ec) {
    return Status::Error(ErrorCode::kNotFound, "input PDF does not exist", request.input_pdf);
  }

  try {
    PoDoFo::PdfMemDocument document;
    document.Load(request.input_pdf);
    result->page_count = document.GetPages().GetCount();
    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kPdfParseFailed, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "failed to parse PDF info");
  }
}

Status DeletePage(const DeletePageRequest& request, DeletePageResult* result) {
  if (request.input_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input path must not be empty");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  Status output_status = PrepareOutputPath(request.output_pdf, request.overwrite);
  if (!output_status.ok()) {
    return output_status;
  }

  try {
    PoDoFo::PdfMemDocument document;
    document.Load(request.input_pdf);
    const uint32_t page_count = document.GetPages().GetCount();

    Status page_status = ValidatePageNumber(request.page_number, page_count, "page_number");
    if (!page_status.ok()) {
      return page_status;
    }
    if (page_count <= 1) {
      return Status::Error(ErrorCode::kUnsupportedFeature, "cannot delete the last page of a document");
    }

    document.GetPages().RemovePageAt(request.page_number - 1);
    document.Save(request.output_pdf);
    result->output_page_count = document.GetPages().GetCount();
    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kPdfParseFailed, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "failed to delete page");
  }
}

Status InsertPage(const InsertPageRequest& request, InsertPageResult* result) {
  if (request.input_pdf.empty() || request.source_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input and source paths must not be empty");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  Status output_status = PrepareOutputPath(request.output_pdf, request.overwrite);
  if (!output_status.ok()) {
    return output_status;
  }

  try {
    PoDoFo::PdfMemDocument target_document;
    target_document.Load(request.input_pdf);
    const uint32_t target_page_count = target_document.GetPages().GetCount();
    if (request.insert_at == 0 || request.insert_at > target_page_count + 1) {
      return Status::Error(ErrorCode::kInvalidArgument, "insert_at must be between 1 and page_count + 1");
    }

    PoDoFo::PdfMemDocument source_document;
    source_document.Load(request.source_pdf);
    const uint32_t source_page_count = source_document.GetPages().GetCount();
    Status page_status = ValidatePageNumber(request.source_page_number, source_page_count, "source_page_number");
    if (!page_status.ok()) {
      return page_status;
    }

    target_document.GetPages().InsertDocumentPageAt(request.insert_at - 1, source_document,
                                                    request.source_page_number - 1);
    target_document.Save(request.output_pdf);
    result->output_page_count = target_document.GetPages().GetCount();
    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kPdfParseFailed, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "failed to insert page");
  }
}

Status ReplacePage(const ReplacePageRequest& request, ReplacePageResult* result) {
  if (request.input_pdf.empty() || request.source_pdf.empty()) {
    return Status::Error(ErrorCode::kInvalidArgument, "input and source paths must not be empty");
  }
  if (result == nullptr) {
    return Status::Error(ErrorCode::kInvalidArgument, "result pointer is null");
  }

  Status output_status = PrepareOutputPath(request.output_pdf, request.overwrite);
  if (!output_status.ok()) {
    return output_status;
  }

  try {
    PoDoFo::PdfMemDocument target_document;
    target_document.Load(request.input_pdf);
    const uint32_t target_page_count = target_document.GetPages().GetCount();
    Status page_status = ValidatePageNumber(request.page_number, target_page_count, "page_number");
    if (!page_status.ok()) {
      return page_status;
    }

    PoDoFo::PdfMemDocument source_document;
    source_document.Load(request.source_pdf);
    const uint32_t source_page_count = source_document.GetPages().GetCount();
    page_status = ValidatePageNumber(request.source_page_number, source_page_count, "source_page_number");
    if (!page_status.ok()) {
      return page_status;
    }

    const unsigned replace_index = request.page_number - 1;
    target_document.GetPages().RemovePageAt(replace_index);
    target_document.GetPages().InsertDocumentPageAt(replace_index, source_document, request.source_page_number - 1);
    target_document.Save(request.output_pdf);
    result->output_page_count = target_document.GetPages().GetCount();
    return Status::Ok();
  } catch (const std::exception& e) {
    return Status::Error(ErrorCode::kPdfParseFailed, e.what());
  } catch (...) {
    return Status::Error(ErrorCode::kPdfParseFailed, "failed to replace page");
  }
}

}  // namespace pdftools::pdf
