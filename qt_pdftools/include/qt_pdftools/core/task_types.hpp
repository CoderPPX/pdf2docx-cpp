#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace qt_pdftools::core {

enum class TaskType {
  kMerge,
  kDeletePage,
  kInsertPage,
  kReplacePage,
  kSwapPages,
  kExtractImages,
  kImagesToPdf,
  kPdfToDocx
};

struct TaskRequest {
  TaskType type = TaskType::kMerge;

  QStringList input_pdfs;
  QString input_pdf;
  QString output_pdf;
  QString output_dir;
  int page = 1;
  int page_b = 1;

  QString source_pdf;
  int at = 1;
  int source_page = 1;

  QString output_docx;
  QString dump_ir_path;
  bool no_images = false;
  bool anchored_images = false;
  QStringList input_images;
};

struct ErrorEnvelope {
  QString code;
  QString message;
  QString context;
  QVariantMap details;
};

struct TaskResult {
  bool ok = false;
  QString backend_id;
  QString backend_label;

  QString stdout_text;
  QString stderr_text;
  QString summary;
  QString output_path;

  QStringList args;
  ErrorEnvelope error;
  QVariantMap details;

  bool fallback_used = false;
  QString fallback_from;
};

struct BackendStatus {
  bool ready = false;
  QString code;
  QString message;
  QVariantMap details;
};

inline QString TaskTypeToString(TaskType type) {
  switch (type) {
    case TaskType::kMerge:
      return QStringLiteral("merge");
    case TaskType::kDeletePage:
      return QStringLiteral("delete-page");
    case TaskType::kInsertPage:
      return QStringLiteral("insert-page");
    case TaskType::kReplacePage:
      return QStringLiteral("replace-page");
    case TaskType::kSwapPages:
      return QStringLiteral("swap-pages");
    case TaskType::kExtractImages:
      return QStringLiteral("extract-images");
    case TaskType::kImagesToPdf:
      return QStringLiteral("image2pdf");
    case TaskType::kPdfToDocx:
      return QStringLiteral("pdf2docx");
  }
  return QStringLiteral("unknown");
}

inline QString TaskTypeToDisplayName(TaskType type) {
  switch (type) {
    case TaskType::kMerge:
      return QStringLiteral("PDF 合并");
    case TaskType::kDeletePage:
      return QStringLiteral("删除页");
    case TaskType::kInsertPage:
      return QStringLiteral("插入页");
    case TaskType::kReplacePage:
      return QStringLiteral("替换页");
    case TaskType::kSwapPages:
      return QStringLiteral("交换页");
    case TaskType::kExtractImages:
      return QStringLiteral("提取图片");
    case TaskType::kImagesToPdf:
      return QStringLiteral("图片转PDF");
    case TaskType::kPdfToDocx:
      return QStringLiteral("PDF -> DOCX");
  }
  return QStringLiteral("未知任务");
}

inline ErrorEnvelope MakeError(QString code,
                               QString message,
                               QString context = {},
                               QVariantMap details = {}) {
  ErrorEnvelope envelope;
  envelope.code = std::move(code);
  envelope.message = std::move(message);
  envelope.context = std::move(context);
  envelope.details = std::move(details);
  return envelope;
}

}  // namespace qt_pdftools::core
