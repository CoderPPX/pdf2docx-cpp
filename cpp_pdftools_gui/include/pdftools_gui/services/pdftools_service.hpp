#pragma once

#include <variant>

#include <QString>

#include "pdftools/convert/pdf2docx.hpp"
#include "pdftools/pdf/create_ops.hpp"
#include "pdftools/pdf/document_ops.hpp"
#include "pdftools/pdf/extract_ops.hpp"

namespace pdftools_gui::services {

enum class OperationKind {
  kMergePdf = 0,
  kExtractText,
  kExtractAttachments,
  kImagesToPdf,
  kDeletePage,
  kInsertPage,
  kReplacePage,
  kPdfToDocx,
};

using RequestVariant = std::variant<pdftools::pdf::MergePdfRequest,
                                    pdftools::pdf::ExtractTextRequest,
                                    pdftools::pdf::ExtractAttachmentsRequest,
                                    pdftools::pdf::ImagesToPdfRequest,
                                    pdftools::pdf::DeletePageRequest,
                                    pdftools::pdf::InsertPageRequest,
                                    pdftools::pdf::ReplacePageRequest,
                                    pdftools::convert::PdfToDocxRequest>;

struct ExecutionOutcome {
  bool success = false;
  QString summary;
  QString detail;
  QString output_path;
};

class PdfToolsService {
 public:
  PdfToolsService() = default;

  ExecutionOutcome Execute(OperationKind op, const RequestVariant& request) const;

  static QString OperationDisplayName(OperationKind op);

 private:
  static QString FormatStatus(const pdftools::Status& status);

  ExecutionOutcome ExecuteMerge(const pdftools::pdf::MergePdfRequest& request) const;
  ExecutionOutcome ExecuteTextExtract(const pdftools::pdf::ExtractTextRequest& request) const;
  ExecutionOutcome ExecuteAttachmentExtract(const pdftools::pdf::ExtractAttachmentsRequest& request) const;
  ExecutionOutcome ExecuteImagesToPdf(const pdftools::pdf::ImagesToPdfRequest& request) const;
  ExecutionOutcome ExecuteDeletePage(const pdftools::pdf::DeletePageRequest& request) const;
  ExecutionOutcome ExecuteInsertPage(const pdftools::pdf::InsertPageRequest& request) const;
  ExecutionOutcome ExecuteReplacePage(const pdftools::pdf::ReplacePageRequest& request) const;
  ExecutionOutcome ExecutePdfToDocx(const pdftools::convert::PdfToDocxRequest& request) const;
};

}  // namespace pdftools_gui::services
