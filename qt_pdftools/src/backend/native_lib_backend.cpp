#include "qt_pdftools/backend/native_lib_backend.hpp"

#include <algorithm>

#include <QStringList>

#include "pdftools/convert/pdf2docx.hpp"
#include "pdftools/error_handling.hpp"
#include "pdftools/pdf/document_ops.hpp"

namespace qt_pdftools::backend {
namespace {

QString ErrorCodeToQtString(pdftools::ErrorCode code) {
  return QString::fromUtf8(pdftools::ErrorCodeToString(code));
}

core::TaskResult BuildSuccess(const QString& backend_id,
                              const QString& backend_label,
                              const QString& summary,
                              const QString& output_path,
                              const QStringList& args,
                              const QVariantMap& details = {}) {
  core::TaskResult result;
  result.ok = true;
  result.backend_id = backend_id;
  result.backend_label = backend_label;
  result.summary = summary;
  result.output_path = output_path;
  result.args = args;
  result.details = details;
  result.stdout_text = summary;
  return result;
}

core::TaskResult BuildFailure(const QString& backend_id,
                              const QString& backend_label,
                              const pdftools::Status& status,
                              const QStringList& args) {
  core::TaskResult result;
  result.ok = false;
  result.backend_id = backend_id;
  result.backend_label = backend_label;
  result.args = args;
  result.error = core::MakeError(
      ErrorCodeToQtString(status.code()),
      QString::fromStdString(status.message()),
      QString::fromStdString(status.context()));
  result.stderr_text = result.error.message;
  result.summary = QStringLiteral("%1 失败").arg(result.error.code);
  return result;
}

}  // namespace

QString NativeLibBackend::id() const {
  return QStringLiteral("native-lib");
}

QString NativeLibBackend::label() const {
  return QStringLiteral("Native Library");
}

core::BackendStatus NativeLibBackend::GetStatus() {
  core::BackendStatus status;
  status.ready = true;
  status.code = QStringLiteral("OK");
  status.message = QStringLiteral("Native library backend 已就绪");
  return status;
}

core::TaskResult NativeLibBackend::RunTask(const core::TaskRequest& request) {
  const QString backend_id = id();
  const QString backend_label = label();

  if (request.type == core::TaskType::kMerge) {
    pdftools::pdf::MergePdfRequest native_req;
    for (const QString& item : request.input_pdfs) {
      native_req.input_pdfs.push_back(item.toStdString());
    }
    native_req.output_pdf = request.output_pdf.toStdString();
    native_req.overwrite = true;

    pdftools::pdf::MergePdfResult native_res;
    const pdftools::Status status = pdftools::pdf::MergePdf(native_req, &native_res);
    const QStringList args = QStringList() << QStringLiteral("merge") << request.output_pdf << request.input_pdfs;
    if (!status.ok()) {
      return BuildFailure(backend_id, backend_label, status, args);
    }

    QVariantMap details;
    details.insert(QStringLiteral("outputPageCount"), static_cast<int>(native_res.output_page_count));
    return BuildSuccess(backend_id,
                        backend_label,
                        QStringLiteral("合并成功，输出页数=%1").arg(native_res.output_page_count),
                        request.output_pdf,
                        args,
                        details);
  }

  if (request.type == core::TaskType::kDeletePage) {
    pdftools::pdf::DeletePageRequest native_req;
    native_req.input_pdf = request.input_pdf.toStdString();
    native_req.output_pdf = request.output_pdf.toStdString();
    native_req.page_number = static_cast<uint32_t>(std::max(1, request.page));
    native_req.overwrite = true;

    pdftools::pdf::DeletePageResult native_res;
    const pdftools::Status status = pdftools::pdf::DeletePage(native_req, &native_res);
    const QStringList args = QStringList() << QStringLiteral("page") << QStringLiteral("delete")
                                           << QStringLiteral("--input") << request.input_pdf
                                           << QStringLiteral("--output") << request.output_pdf
                                           << QStringLiteral("--page") << QString::number(request.page);
    if (!status.ok()) {
      return BuildFailure(backend_id, backend_label, status, args);
    }

    QVariantMap details;
    details.insert(QStringLiteral("outputPageCount"), static_cast<int>(native_res.output_page_count));
    return BuildSuccess(backend_id,
                        backend_label,
                        QStringLiteral("删除页成功，输出页数=%1").arg(native_res.output_page_count),
                        request.output_pdf,
                        args,
                        details);
  }

  if (request.type == core::TaskType::kInsertPage) {
    pdftools::pdf::InsertPageRequest native_req;
    native_req.input_pdf = request.input_pdf.toStdString();
    native_req.output_pdf = request.output_pdf.toStdString();
    native_req.source_pdf = request.source_pdf.toStdString();
    native_req.insert_at = static_cast<uint32_t>(std::max(1, request.at));
    native_req.source_page_number = static_cast<uint32_t>(std::max(1, request.source_page));
    native_req.overwrite = true;

    pdftools::pdf::InsertPageResult native_res;
    const pdftools::Status status = pdftools::pdf::InsertPage(native_req, &native_res);
    const QStringList args = QStringList() << QStringLiteral("page") << QStringLiteral("insert")
                                           << QStringLiteral("--input") << request.input_pdf
                                           << QStringLiteral("--output") << request.output_pdf
                                           << QStringLiteral("--at") << QString::number(request.at)
                                           << QStringLiteral("--source") << request.source_pdf
                                           << QStringLiteral("--source-page") << QString::number(request.source_page);
    if (!status.ok()) {
      return BuildFailure(backend_id, backend_label, status, args);
    }

    QVariantMap details;
    details.insert(QStringLiteral("outputPageCount"), static_cast<int>(native_res.output_page_count));
    return BuildSuccess(backend_id,
                        backend_label,
                        QStringLiteral("插入页成功，输出页数=%1").arg(native_res.output_page_count),
                        request.output_pdf,
                        args,
                        details);
  }

  if (request.type == core::TaskType::kReplacePage) {
    pdftools::pdf::ReplacePageRequest native_req;
    native_req.input_pdf = request.input_pdf.toStdString();
    native_req.output_pdf = request.output_pdf.toStdString();
    native_req.source_pdf = request.source_pdf.toStdString();
    native_req.page_number = static_cast<uint32_t>(std::max(1, request.page));
    native_req.source_page_number = static_cast<uint32_t>(std::max(1, request.source_page));
    native_req.overwrite = true;

    pdftools::pdf::ReplacePageResult native_res;
    const pdftools::Status status = pdftools::pdf::ReplacePage(native_req, &native_res);
    const QStringList args = QStringList() << QStringLiteral("page") << QStringLiteral("replace")
                                           << QStringLiteral("--input") << request.input_pdf
                                           << QStringLiteral("--output") << request.output_pdf
                                           << QStringLiteral("--page") << QString::number(request.page)
                                           << QStringLiteral("--source") << request.source_pdf
                                           << QStringLiteral("--source-page") << QString::number(request.source_page);
    if (!status.ok()) {
      return BuildFailure(backend_id, backend_label, status, args);
    }

    QVariantMap details;
    details.insert(QStringLiteral("outputPageCount"), static_cast<int>(native_res.output_page_count));
    return BuildSuccess(backend_id,
                        backend_label,
                        QStringLiteral("替换页成功，输出页数=%1").arg(native_res.output_page_count),
                        request.output_pdf,
                        args,
                        details);
  }

  if (request.type == core::TaskType::kPdfToDocx) {
    pdftools::convert::PdfToDocxRequest native_req;
    native_req.input_pdf = request.input_pdf.toStdString();
    native_req.output_docx = request.output_docx.toStdString();
    native_req.dump_ir_json_path = request.dump_ir_path.toStdString();
    native_req.extract_images = !request.no_images;
    native_req.use_anchored_images = request.anchored_images;
    native_req.enable_font_fallback = true;
    native_req.overwrite = true;

    pdftools::convert::PdfToDocxResult native_res;
    const pdftools::Status status = pdftools::convert::ConvertPdfToDocx(native_req, &native_res);
    QStringList args = QStringList() << QStringLiteral("convert") << QStringLiteral("pdf2docx")
                                     << QStringLiteral("--input") << request.input_pdf
                                     << QStringLiteral("--output") << request.output_docx;
    if (!request.dump_ir_path.isEmpty()) {
      args << QStringLiteral("--dump-ir") << request.dump_ir_path;
    }
    if (request.no_images) {
      args << QStringLiteral("--no-images");
    }
    if (request.anchored_images) {
      args << QStringLiteral("--docx-anchored");
    }

    if (!status.ok()) {
      return BuildFailure(backend_id, backend_label, status, args);
    }

    QVariantMap details;
    details.insert(QStringLiteral("pageCount"), static_cast<int>(native_res.page_count));
    details.insert(QStringLiteral("imageCount"), static_cast<int>(native_res.image_count));
    details.insert(QStringLiteral("warningCount"), static_cast<int>(native_res.warning_count));
    details.insert(QStringLiteral("backend"), QString::fromStdString(native_res.backend));
    return BuildSuccess(backend_id,
                        backend_label,
                        QStringLiteral("PDF 转 DOCX 成功，页数=%1，图片=%2")
                            .arg(native_res.page_count)
                            .arg(native_res.image_count),
                        request.output_docx,
                        args,
                        details);
  }

  core::TaskResult result;
  result.ok = false;
  result.backend_id = backend_id;
  result.backend_label = backend_label;
  result.error = core::MakeError(QStringLiteral("UNSUPPORTED_FEATURE"),
                                 QStringLiteral("Unsupported task type"),
                                 QStringLiteral("native-lib"));
  result.stderr_text = result.error.message;
  return result;
}

}  // namespace qt_pdftools::backend
