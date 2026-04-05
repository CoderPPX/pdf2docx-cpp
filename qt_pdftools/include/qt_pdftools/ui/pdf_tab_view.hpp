#pragma once

#include <QColor>
#include <QEvent>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QVector>
#include <QWidget>

class QLabel;

#if QT_PDFTOOLS_HAS_QTPDF
class QPdfDocument;
class QPdfView;
#endif

namespace qt_pdftools::ui {

class InkOverlayWidget;

class PdfTabView final : public QWidget {
  Q_OBJECT

 public:
  enum class InkTool {
    kNone = 0,
    kPen,
    kMarker,
    kEraser,
  };

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
  bool ExportPageSnapshot(int page_1_based,
                          const QString& output_png,
                          int max_width = 1800,
                          QString* error_message = nullptr) const;

  void SetInkTool(InkTool tool);
  InkTool ink_tool() const;
  bool UndoInkOnCurrentPage();
  void ClearInkOnCurrentPage();
  bool HasInkOnCurrentPage() const;
  bool HasAnyInk() const;
  bool LoadInkFromJson(const QString& json_path, QString* error_message = nullptr);
  bool SaveInkToJson(const QString& json_path, QString* error_message = nullptr) const;
  void ClearAllInk();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 signals:
  void StateChanged();
  void InkEdited();

 private:
  struct InkStroke {
    int page = 1;
    QColor color;
    qreal width = 2.5;
    qreal opacity = 1.0;
    bool marker = false;
    QVector<QPointF> points_norm;
  };

  friend class InkOverlayWidget;
  void OverlayMousePress(const QPointF& pos, Qt::MouseButton button);
  void OverlayMouseMove(const QPointF& pos, Qt::MouseButtons buttons);
  void OverlayMouseRelease(const QPointF& pos, Qt::MouseButton button);
  void PaintInkOverlay(QPainter* painter, const QRect& rect) const;
  QPointF NormalizeOverlayPoint(const QPointF& pos, const QRect& rect) const;
  QPointF DenormalizeOverlayPoint(const QPointF& normalized, const QRect& rect) const;
  bool TryEraseNearestStroke(const QPointF& pos, const QRect& rect);

  QString file_path_;
  int page_count_ = 0;
  int current_page_ = 1;
  qreal zoom_factor_ = 1.0;

  QLabel* fallback_label_ = nullptr;

#if QT_PDFTOOLS_HAS_QTPDF
  QPdfDocument* document_ = nullptr;
  QPdfView* view_ = nullptr;
  InkOverlayWidget* ink_overlay_ = nullptr;
#endif

  InkTool ink_tool_ = InkTool::kNone;
  QVector<InkStroke> ink_strokes_;
  InkStroke active_stroke_;
  bool has_active_stroke_ = false;
};

}  // namespace qt_pdftools::ui
