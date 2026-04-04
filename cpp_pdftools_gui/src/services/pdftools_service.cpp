#include "pdftools_gui/services/pdftools_service.hpp"

#include <sstream>

namespace pdftools_gui::services {

namespace {

template <typename T>
ExecutionOutcome InvalidRequestType(OperationKind op) {
  ExecutionOutcome outcome;
  outcome.success = false;
  outcome.summary = QStringLiteral("%1 参数类型错误").arg(PdfToolsService::OperationDisplayName(op));
  outcome.detail = QStringLiteral("内部错误：请求类型与操作类型不匹配。");
  return outcome;
}

}  // namespace

ExecutionOutcome PdfToolsService::Execute(OperationKind op, const RequestVariant& request) const {
  switch (op) {
    case OperationKind::kMergePdf:
      if (const auto* typed = std::get_if<pdftools::pdf::MergePdfRequest>(&request)) {
        return ExecuteMerge(*typed);
      }
      return InvalidRequestType<pdftools::pdf::MergePdfRequest>(op);
    case OperationKind::kExtractText:
      if (const auto* typed = std::get_if<pdftools::pdf::ExtractTextRequest>(&request)) {
        return ExecuteTextExtract(*typed);
      }
      return InvalidRequestType<pdftools::pdf::ExtractTextRequest>(op);
    case OperationKind::kExtractAttachments:
      if (const auto* typed = std::get_if<pdftools::pdf::ExtractAttachmentsRequest>(&request)) {
        return ExecuteAttachmentExtract(*typed);
      }
      return InvalidRequestType<pdftools::pdf::ExtractAttachmentsRequest>(op);
    case OperationKind::kImagesToPdf:
      if (const auto* typed = std::get_if<pdftools::pdf::ImagesToPdfRequest>(&request)) {
        return ExecuteImagesToPdf(*typed);
      }
      return InvalidRequestType<pdftools::pdf::ImagesToPdfRequest>(op);
    case OperationKind::kDeletePage:
      if (const auto* typed = std::get_if<pdftools::pdf::DeletePageRequest>(&request)) {
        return ExecuteDeletePage(*typed);
      }
      return InvalidRequestType<pdftools::pdf::DeletePageRequest>(op);
    case OperationKind::kInsertPage:
      if (const auto* typed = std::get_if<pdftools::pdf::InsertPageRequest>(&request)) {
        return ExecuteInsertPage(*typed);
      }
      return InvalidRequestType<pdftools::pdf::InsertPageRequest>(op);
    case OperationKind::kReplacePage:
      if (const auto* typed = std::get_if<pdftools::pdf::ReplacePageRequest>(&request)) {
        return ExecuteReplacePage(*typed);
      }
      return InvalidRequestType<pdftools::pdf::ReplacePageRequest>(op);
    case OperationKind::kPdfToDocx:
      if (const auto* typed = std::get_if<pdftools::convert::PdfToDocxRequest>(&request)) {
        return ExecutePdfToDocx(*typed);
      }
      return InvalidRequestType<pdftools::convert::PdfToDocxRequest>(op);
  }

  ExecutionOutcome outcome;
  outcome.success = false;
  outcome.summary = QStringLiteral("未知操作");
  outcome.detail = QStringLiteral("未识别的 OperationKind 值");
  return outcome;
}

QString PdfToolsService::OperationDisplayName(OperationKind op) {
  switch (op) {
    case OperationKind::kMergePdf:
      return QStringLiteral("PDF 合并");
    case OperationKind::kExtractText:
      return QStringLiteral("文本提取");
    case OperationKind::kExtractAttachments:
      return QStringLiteral("附件提取");
    case OperationKind::kImagesToPdf:
      return QStringLiteral("图片转 PDF");
    case OperationKind::kDeletePage:
      return QStringLiteral("删除页面");
    case OperationKind::kInsertPage:
      return QStringLiteral("插入页面");
    case OperationKind::kReplacePage:
      return QStringLiteral("替换页面");
    case OperationKind::kPdfToDocx:
      return QStringLiteral("PDF 转 DOCX");
  }
  return QStringLiteral("未知操作");
}

QString PdfToolsService::FormatStatus(const pdftools::Status& status) {
  if (status.ok()) {
    return QStringLiteral("成功");
  }
  QString text = QString::fromStdString(status.message());
  if (!status.context().empty()) {
    text += QStringLiteral(" (context: %1)").arg(QString::fromStdString(status.context()));
  }
  return text;
}

ExecutionOutcome PdfToolsService::ExecuteMerge(const pdftools::pdf::MergePdfRequest& request) const {
  pdftools::pdf::MergePdfResult result;
  const pdftools::Status status = pdftools::pdf::MergePdf(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_pdf);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("PDF 合并失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("合并成功，输出页数：%1").arg(result.output_page_count);
  outcome.detail = QStringLiteral("输出文件：%1").arg(outcome.output_path);
  return outcome;
}

ExecutionOutcome PdfToolsService::ExecuteTextExtract(const pdftools::pdf::ExtractTextRequest& request) const {
  pdftools::pdf::ExtractTextResult result;
  const pdftools::Status status = pdftools::pdf::ExtractText(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_path);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("文本提取失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("提取成功，页数=%1，条目=%2").arg(result.page_count).arg(result.entry_count);
  outcome.detail = QStringLiteral("extractor=%1, fallback=%2, 输出=%3")
                       .arg(QString::fromStdString(result.extractor), result.used_fallback ? QStringLiteral("yes")
                                                                                          : QStringLiteral("no"),
                            outcome.output_path);
  return outcome;
}

ExecutionOutcome PdfToolsService::ExecuteAttachmentExtract(const pdftools::pdf::ExtractAttachmentsRequest& request) const {
  pdftools::pdf::ExtractAttachmentsResult result;
  const pdftools::Status status = pdftools::pdf::ExtractAttachments(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_dir);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("附件提取失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("提取完成，附件数=%1").arg(static_cast<int>(result.attachments.size()));

  std::ostringstream oss;
  oss << "parser=" << result.parser << ", parse_failed=" << (result.parse_failed ? "yes" : "no") << "\n";
  for (const auto& attachment : result.attachments) {
    oss << "- " << attachment.filename << " (" << attachment.size_bytes << " B): " << attachment.path << "\n";
  }
  outcome.detail = QString::fromStdString(oss.str());
  return outcome;
}

ExecutionOutcome PdfToolsService::ExecuteImagesToPdf(const pdftools::pdf::ImagesToPdfRequest& request) const {
  pdftools::pdf::ImagesToPdfResult result;
  const pdftools::Status status = pdftools::pdf::ImagesToPdf(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_pdf);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("图片转 PDF 失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("生成成功，页数=%1，跳过图片=%2")
                        .arg(result.page_count)
                        .arg(result.skipped_image_count);
  outcome.detail = QStringLiteral("输出文件：%1").arg(outcome.output_path);
  return outcome;
}

ExecutionOutcome PdfToolsService::ExecuteDeletePage(const pdftools::pdf::DeletePageRequest& request) const {
  pdftools::pdf::DeletePageResult result;
  const pdftools::Status status = pdftools::pdf::DeletePage(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_pdf);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("删除页面失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("删除成功，输出页数=%1").arg(result.output_page_count);
  outcome.detail = QStringLiteral("输出文件：%1").arg(outcome.output_path);
  return outcome;
}

ExecutionOutcome PdfToolsService::ExecuteInsertPage(const pdftools::pdf::InsertPageRequest& request) const {
  pdftools::pdf::InsertPageResult result;
  const pdftools::Status status = pdftools::pdf::InsertPage(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_pdf);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("插入页面失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("插入成功，输出页数=%1").arg(result.output_page_count);
  outcome.detail = QStringLiteral("输出文件：%1").arg(outcome.output_path);
  return outcome;
}

ExecutionOutcome PdfToolsService::ExecuteReplacePage(const pdftools::pdf::ReplacePageRequest& request) const {
  pdftools::pdf::ReplacePageResult result;
  const pdftools::Status status = pdftools::pdf::ReplacePage(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_pdf);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("替换页面失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("替换成功，输出页数=%1").arg(result.output_page_count);
  outcome.detail = QStringLiteral("输出文件：%1").arg(outcome.output_path);
  return outcome;
}

ExecutionOutcome PdfToolsService::ExecutePdfToDocx(const pdftools::convert::PdfToDocxRequest& request) const {
  pdftools::convert::PdfToDocxResult result;
  const pdftools::Status status = pdftools::convert::ConvertPdfToDocx(request, &result);

  ExecutionOutcome outcome;
  outcome.output_path = QString::fromStdString(request.output_docx);
  if (!status.ok()) {
    outcome.success = false;
    outcome.summary = QStringLiteral("PDF 转 DOCX 失败");
    outcome.detail = FormatStatus(status);
    return outcome;
  }

  outcome.success = true;
  outcome.summary = QStringLiteral("转换成功，页数=%1，图片=%2，警告=%3")
                        .arg(result.page_count)
                        .arg(result.image_count)
                        .arg(result.warning_count);
  outcome.detail = QStringLiteral("backend=%1, extracted=%2, skipped=%3, 输出=%4")
                       .arg(QString::fromStdString(result.backend))
                       .arg(result.extracted_image_count)
                       .arg(result.skipped_image_count)
                       .arg(outcome.output_path);
  return outcome;
}

}  // namespace pdftools_gui::services
