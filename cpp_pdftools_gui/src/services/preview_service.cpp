#include "pdftools_gui/services/preview_service.hpp"

#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QFileInfo>
#include <QImageReader>
#include <QTextStream>

#include "pdf2docx/converter.hpp"
#include "pdftools/pdf/document_ops.hpp"

namespace pdftools_gui::services {

namespace {

QString EscapeHtml(const std::string& text) {
  return QString::fromStdString(text).toHtmlEscaped();
}

QString BuildIrHtml(const pdf2docx::ir::Document& doc,
                    int only_page,
                    bool include_images,
                    double scale,
                    int max_span_count,
                    int max_image_count,
                    int* rendered_pages,
                    int* rendered_spans,
                    int* rendered_images) {
  int page_begin = 0;
  int page_end = static_cast<int>(doc.pages.size());
  if (only_page > 0 && only_page <= static_cast<int>(doc.pages.size())) {
    page_begin = only_page - 1;
    page_end = only_page;
  }

  const double normalized_scale = std::clamp(scale, 0.2, 3.0);
  int span_budget = std::max(0, max_span_count);
  int image_budget = std::max(0, max_image_count);

  QString html;
  QTextStream stream(&html);
  stream << "<html><head><meta charset=\"utf-8\"/>"
         << "<style>"
         << "body{font-family:'Noto Sans CJK SC','Microsoft YaHei',sans-serif;background:#f4f6f8;color:#1f2937;}"
         << ".page{margin:12px 0;padding:10px;background:#fff;border:1px solid #d1d5db;border-radius:8px;}"
         << ".title{font-weight:600;font-size:13px;margin-bottom:8px;}"
         << ".canvas{position:relative;border:1px dashed #9ca3af;background:#fffdf7;overflow:hidden;}"
         << ".txt{position:absolute;font-size:11px;line-height:1.2;color:#111827;white-space:nowrap;}"
         << ".img{position:absolute;border:1px solid rgba(59,130,246,0.35);object-fit:fill;background:#eef2ff;}"
         << ".warn{font-size:12px;color:#b45309;margin-top:6px;}"
         << "</style></head><body>";

  int pages = 0;
  int spans = 0;
  int images = 0;
  for (int page_index = page_begin; page_index < page_end; ++page_index) {
    const auto& page = doc.pages.at(static_cast<size_t>(page_index));
    const double canvas_w = std::max(1.0, page.width_pt * normalized_scale);
    const double canvas_h = std::max(1.0, page.height_pt * normalized_scale);

    stream << "<div class=\"page\">";
    stream << "<div class=\"title\">Page " << (page_index + 1) << " | "
           << page.spans.size() << " spans | " << page.images.size() << " images</div>";
    stream << "<div class=\"canvas\" style=\"width:" << canvas_w << "px;height:" << canvas_h << "px;\">";

    for (const auto& span : page.spans) {
      if (span_budget <= 0) {
        break;
      }
      const double left_pt = span.has_bbox ? span.bbox.x : span.x;
      const double top_base_pt = span.has_bbox ? (span.bbox.y + span.bbox.height) : span.y;
      const double top_pt = std::max(0.0, page.height_pt - top_base_pt);
      const double width_pt = span.has_bbox ? std::max(4.0, span.bbox.width) : std::max(6.0, span.length * 5.0);
      const double height_pt = span.has_bbox ? std::max(8.0, span.bbox.height) : 12.0;

      stream << "<div class=\"txt\" style=\"left:" << (left_pt * normalized_scale) << "px;top:"
             << (top_pt * normalized_scale) << "px;min-width:" << (width_pt * normalized_scale)
             << "px;height:" << (height_pt * normalized_scale) << "px;\">"
             << EscapeHtml(span.text) << "</div>";
      --span_budget;
      ++spans;
    }

    if (include_images) {
      for (const auto& image : page.images) {
        if (image_budget <= 0) {
          break;
        }
        if (image.data.empty()) {
          continue;
        }
        const QByteArray bytes(reinterpret_cast<const char*>(image.data.data()),
                               static_cast<int>(image.data.size()));
        const QByteArray base64 = bytes.toBase64();
        const double top_pt = std::max(0.0, page.height_pt - image.y - image.height);
        stream << "<img class=\"img\" style=\"left:" << (image.x * normalized_scale) << "px;top:"
               << (top_pt * normalized_scale) << "px;width:" << (std::max(1.0, image.width) * normalized_scale)
               << "px;height:" << (std::max(1.0, image.height) * normalized_scale) << "px;\" src=\"data:"
               << QString::fromStdString(image.mime_type).toHtmlEscaped() << ";base64,"
               << QString::fromLatin1(base64) << "\"/>";
        --image_budget;
        ++images;
      }
    }

    stream << "</div>";
    if (span_budget <= 0 || image_budget <= 0) {
      stream << "<div class=\"warn\">Preview truncated for performance limits.</div>";
    }
    stream << "</div>";
    ++pages;
  }

  stream << "</body></html>";

  if (rendered_pages != nullptr) {
    *rendered_pages = pages;
  }
  if (rendered_spans != nullptr) {
    *rendered_spans = spans;
  }
  if (rendered_images != nullptr) {
    *rendered_images = images;
  }
  return html;
}

QImage BuildScaledImage(const QImage& source, int thumb_edge) {
  if (source.isNull()) {
    return {};
  }
  return source.scaled(thumb_edge,
                       thumb_edge,
                       Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);
}

}  // namespace

int PreviewService::QueryPdfPageCount(const QString& input_pdf, QString* error_message) const {
  if (error_message != nullptr) {
    error_message->clear();
  }
  const QString path = input_pdf.trimmed();
  if (path.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("输入路径为空");
    }
    return 0;
  }

  pdftools::pdf::PdfInfoRequest request;
  request.input_pdf = path.toStdString();
  pdftools::pdf::PdfInfoResult result;
  const pdftools::Status status = pdftools::pdf::GetPdfInfo(request, &result);
  if (!status.ok()) {
    if (error_message != nullptr) {
      *error_message = QString::fromStdString(status.message());
    }
    return 0;
  }
  return static_cast<int>(result.page_count);
}

IrPreviewResult PreviewService::RenderPdfIrPreview(const QString& input_pdf,
                                                   int only_page,
                                                   bool include_images,
                                                   double scale,
                                                   int max_span_count,
                                                   int max_image_count) const {
  IrPreviewResult output;
  const QString path = input_pdf.trimmed();
  if (path.isEmpty()) {
    output.message = QStringLiteral("输入路径为空");
    return output;
  }
  if (!QFileInfo::exists(path)) {
    output.message = QStringLiteral("文件不存在");
    return output;
  }

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  options.extract_images = include_images;
  options.enable_font_fallback = true;

  pdf2docx::ir::Document doc;
  pdf2docx::ImageExtractionStats image_stats;
  const pdf2docx::Status status =
      converter.ExtractIrFromFile(path.toStdString(), options, &doc, &image_stats);
  if (!status.ok()) {
    output.message = QString::fromStdString(status.message());
    return output;
  }

  output.total_pages = static_cast<int>(doc.pages.size());
  if (output.total_pages == 0) {
    output.success = true;
    output.message = QStringLiteral("PDF 没有页面内容");
    output.html = QStringLiteral("<html><body><p>Empty document.</p></body></html>");
    return output;
  }

  if (only_page < 0 || only_page > output.total_pages) {
    output.message = QStringLiteral("预览页码超出范围");
    return output;
  }

  output.html = BuildIrHtml(doc,
                            only_page,
                            include_images,
                            scale,
                            max_span_count,
                            max_image_count,
                            &output.rendered_pages,
                            &output.span_count,
                            &output.image_count);
  output.success = true;
  output.message = QStringLiteral("Preview ready: pages=%1 spans=%2 images=%3")
                       .arg(output.rendered_pages)
                       .arg(output.span_count)
                       .arg(output.image_count);
  return output;
}

QVector<ImageThumbnail> PreviewService::BuildImageThumbnails(const QStringList& image_paths,
                                                             int thumb_edge,
                                                             int* unreadable_count) const {
  QVector<ImageThumbnail> thumbnails;
  int local_unreadable = 0;

  const int edge = std::clamp(thumb_edge, 48, 512);
  for (const QString& path : image_paths) {
    QImageReader reader(path);
    QImage image = reader.read();
    if (image.isNull()) {
      ++local_unreadable;
      continue;
    }
    ImageThumbnail thumb;
    thumb.image = BuildScaledImage(image, edge);
    thumb.label =
        QStringLiteral("%1 (%2x%3)").arg(QFileInfo(path).fileName()).arg(image.width()).arg(image.height());
    thumbnails.push_back(std::move(thumb));
  }

  if (unreadable_count != nullptr) {
    *unreadable_count = local_unreadable;
  }
  return thumbnails;
}

QVector<ImageThumbnail> PreviewService::ExtractImageThumbnailsFromPdf(const QString& input_pdf,
                                                                      int only_page,
                                                                      int thumb_edge,
                                                                      int max_image_count,
                                                                      QString* error_message) const {
  if (error_message != nullptr) {
    error_message->clear();
  }
  QVector<ImageThumbnail> thumbnails;

  const QString path = input_pdf.trimmed();
  if (path.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("输入路径为空");
    }
    return thumbnails;
  }

  pdf2docx::Converter converter(pdf2docx::BackendKind::kPoDoFo);
  pdf2docx::ConvertOptions options;
  options.extract_images = true;
  options.enable_font_fallback = true;

  pdf2docx::ir::Document doc;
  pdf2docx::ImageExtractionStats image_stats;
  const pdf2docx::Status status =
      converter.ExtractIrFromFile(path.toStdString(), options, &doc, &image_stats);
  if (!status.ok()) {
    if (error_message != nullptr) {
      *error_message = QString::fromStdString(status.message());
    }
    return thumbnails;
  }

  const int edge = std::clamp(thumb_edge, 48, 512);
  const int image_cap = std::max(1, max_image_count);
  int page_begin = 0;
  int page_end = static_cast<int>(doc.pages.size());
  if (only_page > 0 && only_page <= page_end) {
    page_begin = only_page - 1;
    page_end = only_page;
  }

  for (int page_index = page_begin; page_index < page_end; ++page_index) {
    const auto& page = doc.pages.at(static_cast<size_t>(page_index));
    for (size_t image_index = 0; image_index < page.images.size(); ++image_index) {
      if (thumbnails.size() >= image_cap) {
        return thumbnails;
      }
      const auto& image_block = page.images.at(image_index);
      if (image_block.data.empty()) {
        continue;
      }
      const QByteArray bytes(reinterpret_cast<const char*>(image_block.data.data()),
                             static_cast<int>(image_block.data.size()));
      QImage image;
      if (!image.loadFromData(bytes)) {
        continue;
      }

      ImageThumbnail thumb;
      thumb.image = BuildScaledImage(image, edge);
      thumb.label = QStringLiteral("P%1-#%2 (%3x%4)")
                        .arg(page_index + 1)
                        .arg(static_cast<int>(image_index + 1))
                        .arg(image.width())
                        .arg(image.height());
      thumbnails.push_back(std::move(thumb));
    }
  }

  return thumbnails;
}

}  // namespace pdftools_gui::services
