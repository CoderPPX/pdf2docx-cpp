#pragma once

#include <QWidget>

class QLabel;

#if QT_PDFTOOLS_HAS_QTPDF
class QPdfDocument;
class QPdfView;
#endif

namespace qt_pdftools::ui {

class PdfTabView final : public QWidget {
  Q_OBJECT

 public:
  explicit PdfTabView(QWidget* parent = nullptr);
  ~PdfTabView() override;

  bool LoadFile(const QString& path, QString* error_message = nullptr);

  QString file_path() const;
  int page_count() const;
  int current_page() const;  // 1-based
  qreal zoom_factor() const;

  void SetCurrentPage(int page_1_based);
  void NextPage();
  void PrevPage();
  void ZoomIn();
  void ZoomOut();

 signals:
  void StateChanged();

 private:
  QString file_path_;
  int page_count_ = 0;
  int current_page_ = 1;
  qreal zoom_factor_ = 1.0;

  QLabel* fallback_label_ = nullptr;

#if QT_PDFTOOLS_HAS_QTPDF
  QPdfDocument* document_ = nullptr;
  QPdfView* view_ = nullptr;
#endif
};

}  // namespace qt_pdftools::ui
