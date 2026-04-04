#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

namespace pdftools_gui::services {

struct ImageThumbnail {
  QImage image;
  QString label;
};

struct IrPreviewResult {
  bool success = false;
  QString message;
  QString html;
  int total_pages = 0;
  int rendered_pages = 0;
  int span_count = 0;
  int image_count = 0;
};

class PreviewService {
 public:
  PreviewService() = default;

  int QueryPdfPageCount(const QString& input_pdf, QString* error_message = nullptr) const;

  IrPreviewResult RenderPdfIrPreview(const QString& input_pdf,
                                     int only_page = 0,
                                     bool include_images = true,
                                     double scale = 1.0,
                                     int max_span_count = 800,
                                     int max_image_count = 120) const;

  QVector<ImageThumbnail> BuildImageThumbnails(const QStringList& image_paths,
                                               int thumb_edge = 160,
                                               int* unreadable_count = nullptr) const;

  QVector<ImageThumbnail> ExtractImageThumbnailsFromPdf(const QString& input_pdf,
                                                        int only_page = 0,
                                                        int thumb_edge = 160,
                                                        int max_image_count = 120,
                                                        QString* error_message = nullptr) const;
};

}  // namespace pdftools_gui::services
