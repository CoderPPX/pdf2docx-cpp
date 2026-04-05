#include "qt_pdftools/ui/pdf_tab_view.hpp"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>
#include <QScrollBar>
#include <QVBoxLayout>

#if QT_PDFTOOLS_HAS_QTPDF
#include <QtPdf/qpdfdocument.h>
#include <QtPdf/qpdfpagenavigator.h>
#include <QtPdfWidgets/qpdfview.h>
#endif

namespace qt_pdftools::ui {

#if QT_PDFTOOLS_HAS_QTPDF
class InkOverlayWidget final : public QWidget {
 public:
  InkOverlayWidget(PdfTabView* owner, QWidget* parent) : QWidget(parent), owner_(owner) {
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    SetInputEnabled(false);
  }

  void SetInputEnabled(bool enabled) {
    setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
  }

 protected:
  void paintEvent(QPaintEvent* event) override {
    Q_UNUSED(event)
    if (owner_ == nullptr) {
      return;
    }
    QPainter painter(this);
    owner_->PaintInkOverlay(&painter, rect());
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (owner_ == nullptr) {
      return;
    }
    owner_->OverlayMousePress(event->position(), event->button());
    event->accept();
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    if (owner_ == nullptr) {
      return;
    }
    owner_->OverlayMouseMove(event->position(), event->buttons());
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    if (owner_ == nullptr) {
      return;
    }
    owner_->OverlayMouseRelease(event->position(), event->button());
    event->accept();
  }

 private:
  PdfTabView* owner_ = nullptr;
};
#endif

PdfTabView::PdfTabView(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);

#if QT_PDFTOOLS_HAS_QTPDF
  document_ = new QPdfDocument(this);
  view_ = new QPdfView(this);
  view_->setDocument(document_);
  view_->setPageMode(QPdfView::PageMode::MultiPage);
  view_->setZoomMode(QPdfView::ZoomMode::Custom);
  view_->setZoomFactor(1.15);
  root->addWidget(view_, 1);

  if (view_->viewport() != nullptr) {
    ink_overlay_ = new InkOverlayWidget(this, view_->viewport());
    ink_overlay_->setGeometry(view_->viewport()->rect());
    ink_overlay_->raise();
    view_->viewport()->installEventFilter(this);
  }

  if (view_->pageNavigator() != nullptr) {
    connect(view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this, [this](int page_index) {
      current_page_ = std::max(1, page_index + 1);
      if (ink_overlay_ != nullptr) {
        ink_overlay_->update();
      }
      emit StateChanged();
    });
  }

  connect(view_, &QPdfView::zoomFactorChanged, this, [this](qreal value) {
    zoom_factor_ = std::clamp(value, 0.2, 5.0);
    if (ink_overlay_ != nullptr) {
      ink_overlay_->update();
    }
    emit StateChanged();
  });

  connect(view_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
    if (ink_overlay_ != nullptr) {
      ink_overlay_->update();
    }
  });
  connect(view_->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
    if (ink_overlay_ != nullptr) {
      ink_overlay_->update();
    }
  });
#else
  fallback_label_ = new QLabel(this);
  fallback_label_->setWordWrap(true);
  fallback_label_->setText(QStringLiteral("Qt6Pdf 未启用，当前仅显示文件信息占位。"));
  root->addWidget(fallback_label_, 1);
#endif
}

PdfTabView::~PdfTabView() {
#if QT_PDFTOOLS_HAS_QTPDF
  if (view_ != nullptr) {
    if (view_->viewport() != nullptr) {
      view_->viewport()->removeEventFilter(this);
    }
    view_->setDocument(nullptr);
  }
  if (document_ != nullptr) {
    document_->close();
  }
#endif
}

bool PdfTabView::eventFilter(QObject* watched, QEvent* event) {
#if QT_PDFTOOLS_HAS_QTPDF
  if (view_ != nullptr && view_->viewport() != nullptr && watched == view_->viewport() && ink_overlay_ != nullptr) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
      ink_overlay_->setGeometry(view_->viewport()->rect());
      ink_overlay_->raise();
    }
  }
#else
  Q_UNUSED(watched)
  Q_UNUSED(event)
#endif
  return QWidget::eventFilter(watched, event);
}

bool PdfTabView::LoadFile(const QString& path, QString* error_message) {
  const QString normalized = path.trimmed();
  if (error_message != nullptr) {
    error_message->clear();
  }
  if (normalized.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("路径为空");
    }
    return false;
  }
  if (!QFileInfo::exists(normalized)) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("文件不存在");
    }
    return false;
  }

#if QT_PDFTOOLS_HAS_QTPDF
  const QPdfDocument::Error err = document_->load(normalized);
  if (err != QPdfDocument::Error::None) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("加载 PDF 失败: %1").arg(static_cast<int>(err));
    }
    return false;
  }

  file_path_ = normalized;
  page_count_ = std::max(0, document_->pageCount());
  current_page_ = page_count_ > 0 ? 1 : 0;
  zoom_factor_ = view_->zoomFactor();
  has_active_stroke_ = false;
  if (view_->pageNavigator() != nullptr && page_count_ > 0) {
    view_->pageNavigator()->jump(0, QPointF(), zoom_factor_);
  }
  if (ink_overlay_ != nullptr) {
    ink_overlay_->update();
  }
#else
  file_path_ = normalized;
  page_count_ = 0;
  current_page_ = 0;
  zoom_factor_ = 1.0;
  if (fallback_label_ != nullptr) {
    fallback_label_->setText(QStringLiteral("占位预览\n文件: %1").arg(normalized));
  }
#endif

  emit StateChanged();
  return true;
}

QString PdfTabView::file_path() const {
  return file_path_;
}

int PdfTabView::page_count() const {
#if QT_PDFTOOLS_HAS_QTPDF
  return std::max(0, document_->pageCount());
#else
  return page_count_;
#endif
}

int PdfTabView::current_page() const {
  return current_page_;
}

qreal PdfTabView::zoom_factor() const {
#if QT_PDFTOOLS_HAS_QTPDF
  return view_ != nullptr ? view_->zoomFactor() : zoom_factor_;
#else
  return zoom_factor_;
#endif
}

void PdfTabView::SetCurrentPage(int page_1_based) {
#if QT_PDFTOOLS_HAS_QTPDF
  const int total = page_count();
  if (view_ == nullptr || view_->pageNavigator() == nullptr || total <= 0) {
    return;
  }
  const int clamped = std::clamp(page_1_based, 1, total);
  view_->pageNavigator()->jump(clamped - 1, QPointF(), zoom_factor());
#else
  Q_UNUSED(page_1_based)
#endif
}

void PdfTabView::NextPage() {
  SetCurrentPage(current_page() + 1);
}

void PdfTabView::PrevPage() {
  SetCurrentPage(current_page() - 1);
}

void PdfTabView::ZoomIn() {
#if QT_PDFTOOLS_HAS_QTPDF
  if (view_ == nullptr) {
    return;
  }
  view_->setZoomMode(QPdfView::ZoomMode::Custom);
  view_->setZoomFactor(std::clamp(view_->zoomFactor() + 0.1, 0.2, 5.0));
#else
  zoom_factor_ = std::clamp(zoom_factor_ + 0.1, 0.2, 5.0);
  emit StateChanged();
#endif
}

void PdfTabView::ZoomOut() {
#if QT_PDFTOOLS_HAS_QTPDF
  if (view_ == nullptr) {
    return;
  }
  view_->setZoomMode(QPdfView::ZoomMode::Custom);
  view_->setZoomFactor(std::clamp(view_->zoomFactor() - 0.1, 0.2, 5.0));
#else
  zoom_factor_ = std::clamp(zoom_factor_ - 0.1, 0.2, 5.0);
  emit StateChanged();
#endif
}

bool PdfTabView::ExportPageSnapshot(int page_1_based,
                                    const QString& output_png,
                                    int max_width,
                                    QString* error_message) const {
  if (error_message != nullptr) {
    error_message->clear();
  }

  const QString output_path = output_png.trimmed();
  if (output_path.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("输出路径为空");
    }
    return false;
  }

#if QT_PDFTOOLS_HAS_QTPDF
  if (document_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("PDF 文档未初始化");
    }
    return false;
  }
  const int total = document_->pageCount();
  if (total <= 0) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("当前文档没有可导出页面");
    }
    return false;
  }

  const int clamped_page = std::clamp(page_1_based, 1, total);
  const int page_index = clamped_page - 1;
  const QSizeF page_size_pt = document_->pagePointSize(page_index);
  if (!page_size_pt.isValid() || page_size_pt.width() <= 0.0 || page_size_pt.height() <= 0.0) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("无法读取页面尺寸");
    }
    return false;
  }

  const int target_width = std::max(256, max_width);
  const qreal aspect = page_size_pt.height() / page_size_pt.width();
  const int target_height = std::max(256, static_cast<int>(target_width * aspect));
  const QImage image = document_->render(page_index, QSize(target_width, target_height));
  if (image.isNull()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("页面渲染失败");
    }
    return false;
  }
  if (!image.save(output_path, "PNG")) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("PNG 写入失败");
    }
    return false;
  }
  return true;
#else
  Q_UNUSED(page_1_based)
  Q_UNUSED(max_width)
  if (error_message != nullptr) {
    *error_message = QStringLiteral("当前构建未启用 QtPdf，无法导出页面图像");
  }
  return false;
#endif
}

void PdfTabView::SetInkTool(PdfTabView::InkTool tool) {
  ink_tool_ = tool;
#if QT_PDFTOOLS_HAS_QTPDF
  if (ink_overlay_ != nullptr) {
    ink_overlay_->SetInputEnabled(ink_tool_ != InkTool::kNone);
    ink_overlay_->update();
  }
#endif
}

PdfTabView::InkTool PdfTabView::ink_tool() const {
  return ink_tool_;
}

bool PdfTabView::UndoInkOnCurrentPage() {
  for (int i = ink_strokes_.size() - 1; i >= 0; --i) {
    if (ink_strokes_[i].page == current_page_) {
      ink_strokes_.removeAt(i);
#if QT_PDFTOOLS_HAS_QTPDF
      if (ink_overlay_ != nullptr) {
        ink_overlay_->update();
      }
#endif
      emit InkEdited();
      return true;
    }
  }
  return false;
}

void PdfTabView::ClearInkOnCurrentPage() {
  bool removed = false;
  for (int i = ink_strokes_.size() - 1; i >= 0; --i) {
    if (ink_strokes_[i].page == current_page_) {
      ink_strokes_.removeAt(i);
      removed = true;
    }
  }
  if (removed) {
#if QT_PDFTOOLS_HAS_QTPDF
    if (ink_overlay_ != nullptr) {
      ink_overlay_->update();
    }
#endif
    emit InkEdited();
  }
}

bool PdfTabView::HasInkOnCurrentPage() const {
  for (const InkStroke& stroke : ink_strokes_) {
    if (stroke.page == current_page_) {
      return true;
    }
  }
  return false;
}

bool PdfTabView::HasAnyInk() const {
  return !ink_strokes_.isEmpty();
}

bool PdfTabView::LoadInkFromJson(const QString& json_path, QString* error_message) {
  if (error_message != nullptr) {
    error_message->clear();
  }

  const QString path = json_path.trimmed();
  if (path.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("墨迹路径为空");
    }
    return false;
  }
  if (!QFileInfo::exists(path)) {
    ink_strokes_.clear();
#if QT_PDFTOOLS_HAS_QTPDF
    if (ink_overlay_ != nullptr) {
      ink_overlay_->update();
    }
#endif
    return true;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("打开墨迹文件失败");
    }
    return false;
  }
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isArray()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("墨迹文件格式错误");
    }
    return false;
  }

  QVector<InkStroke> strokes;
  for (const QJsonValue& value : doc.array()) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    InkStroke stroke;
    stroke.page = std::max(1, obj.value(QStringLiteral("page")).toInt(1));
    stroke.color = QColor(obj.value(QStringLiteral("color")).toString(QStringLiteral("#2f6fed")));
    stroke.width = std::max(0.5, obj.value(QStringLiteral("width")).toDouble(2.5));
    stroke.opacity = std::clamp(obj.value(QStringLiteral("opacity")).toDouble(1.0), 0.05, 1.0);
    stroke.marker = obj.value(QStringLiteral("marker")).toBool(false);

    const QJsonArray points = obj.value(QStringLiteral("points")).toArray();
    for (const QJsonValue& pt_value : points) {
      if (!pt_value.isArray()) {
        continue;
      }
      const QJsonArray pt = pt_value.toArray();
      if (pt.size() < 2) {
        continue;
      }
      const qreal x = std::clamp(pt[0].toDouble(), 0.0, 1.0);
      const qreal y = std::clamp(pt[1].toDouble(), 0.0, 1.0);
      stroke.points_norm.push_back(QPointF(x, y));
    }
    if (stroke.points_norm.size() >= 2) {
      strokes.push_back(std::move(stroke));
    }
  }

  ink_strokes_ = std::move(strokes);
  has_active_stroke_ = false;
#if QT_PDFTOOLS_HAS_QTPDF
  if (ink_overlay_ != nullptr) {
    ink_overlay_->update();
  }
#endif
  return true;
}

bool PdfTabView::SaveInkToJson(const QString& json_path, QString* error_message) const {
  if (error_message != nullptr) {
    error_message->clear();
  }

  const QString path = json_path.trimmed();
  if (path.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("墨迹路径为空");
    }
    return false;
  }

  if (ink_strokes_.isEmpty()) {
    if (QFileInfo::exists(path)) {
      QFile::remove(path);
    }
    return true;
  }

  const QString parent_dir = QFileInfo(path).absolutePath();
  if (!parent_dir.isEmpty()) {
    QDir().mkpath(parent_dir);
  }

  QJsonArray root;
  for (const InkStroke& stroke : ink_strokes_) {
    QJsonObject obj;
    obj.insert(QStringLiteral("page"), stroke.page);
    obj.insert(QStringLiteral("color"), stroke.color.name(QColor::HexRgb));
    obj.insert(QStringLiteral("width"), stroke.width);
    obj.insert(QStringLiteral("opacity"), stroke.opacity);
    obj.insert(QStringLiteral("marker"), stroke.marker);
    QJsonArray points;
    for (const QPointF& pt : stroke.points_norm) {
      points.push_back(QJsonArray{pt.x(), pt.y()});
    }
    obj.insert(QStringLiteral("points"), points);
    root.push_back(obj);
  }

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("创建墨迹文件失败");
    }
    return false;
  }
  const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (file.write(data) < 0 || !file.commit()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("写入墨迹文件失败");
    }
    return false;
  }
  return true;
}

void PdfTabView::ClearAllInk() {
  if (ink_strokes_.isEmpty()) {
    return;
  }
  ink_strokes_.clear();
  has_active_stroke_ = false;
#if QT_PDFTOOLS_HAS_QTPDF
  if (ink_overlay_ != nullptr) {
    ink_overlay_->update();
  }
#endif
  emit InkEdited();
}

void PdfTabView::OverlayMousePress(const QPointF& pos, Qt::MouseButton button) {
  if (button != Qt::LeftButton || ink_tool_ == InkTool::kNone) {
    return;
  }
#if QT_PDFTOOLS_HAS_QTPDF
  if (ink_overlay_ == nullptr) {
    return;
  }
  const QRect rect = ink_overlay_->rect();
  if (!rect.contains(pos.toPoint())) {
    return;
  }
#else
  Q_UNUSED(pos)
  Q_UNUSED(button)
  return;
#endif

  if (ink_tool_ == InkTool::kEraser) {
#if QT_PDFTOOLS_HAS_QTPDF
    if (TryEraseNearestStroke(pos, rect)) {
      emit InkEdited();
      ink_overlay_->update();
    }
#endif
    return;
  }

  has_active_stroke_ = true;
  active_stroke_ = InkStroke{};
  active_stroke_.page = std::max(1, current_page_);
  active_stroke_.marker = ink_tool_ == InkTool::kMarker;
  if (ink_tool_ == InkTool::kMarker) {
    active_stroke_.color = QColor(QStringLiteral("#f8c044"));
    active_stroke_.width = 14.0;
    active_stroke_.opacity = 0.35;
  } else {
    active_stroke_.color = QColor(QStringLiteral("#2f6fed"));
    active_stroke_.width = 2.6;
    active_stroke_.opacity = 1.0;
  }
#if QT_PDFTOOLS_HAS_QTPDF
  active_stroke_.points_norm.push_back(NormalizeOverlayPoint(pos, rect));
  ink_overlay_->update();
#endif
}

void PdfTabView::OverlayMouseMove(const QPointF& pos, Qt::MouseButtons buttons) {
  if (!has_active_stroke_ || ink_tool_ == InkTool::kNone) {
    return;
  }
  if ((buttons & Qt::LeftButton) == 0) {
    return;
  }
#if QT_PDFTOOLS_HAS_QTPDF
  if (ink_overlay_ == nullptr) {
    return;
  }
  const QRect rect = ink_overlay_->rect();
  const QPointF norm = NormalizeOverlayPoint(pos, rect);
  if (!active_stroke_.points_norm.isEmpty()) {
    const QPointF last = active_stroke_.points_norm.back();
    const qreal dx = norm.x() - last.x();
    const qreal dy = norm.y() - last.y();
    if ((dx * dx + dy * dy) < 1e-6) {
      return;
    }
  }
  active_stroke_.points_norm.push_back(norm);
  ink_overlay_->update();
#else
  Q_UNUSED(pos)
#endif
}

void PdfTabView::OverlayMouseRelease(const QPointF& pos, Qt::MouseButton button) {
  if (!has_active_stroke_ || button != Qt::LeftButton) {
    return;
  }
  has_active_stroke_ = false;
#if QT_PDFTOOLS_HAS_QTPDF
  if (ink_overlay_ == nullptr) {
    return;
  }
  active_stroke_.points_norm.push_back(NormalizeOverlayPoint(pos, ink_overlay_->rect()));
#endif
  if (active_stroke_.points_norm.size() < 2) {
    return;
  }
  ink_strokes_.push_back(active_stroke_);
#if QT_PDFTOOLS_HAS_QTPDF
  ink_overlay_->update();
#endif
  emit InkEdited();
}

void PdfTabView::PaintInkOverlay(QPainter* painter, const QRect& rect) const {
  if (painter == nullptr) {
    return;
  }
  painter->setRenderHint(QPainter::Antialiasing, true);

  const auto draw_stroke = [this, painter, rect](const InkStroke& stroke) {
    if (stroke.points_norm.size() < 2 || stroke.page != current_page_) {
      return;
    }

    QColor color = stroke.color;
    color.setAlphaF(stroke.opacity);
    QPen pen(color,
             stroke.width,
             Qt::SolidLine,
             Qt::RoundCap,
             Qt::RoundJoin);
    painter->setPen(pen);
    painter->setCompositionMode(stroke.marker ? QPainter::CompositionMode_Multiply
                                              : QPainter::CompositionMode_SourceOver);

    QPainterPath path;
    path.moveTo(DenormalizeOverlayPoint(stroke.points_norm.front(), rect));
    for (int i = 1; i < stroke.points_norm.size(); ++i) {
      path.lineTo(DenormalizeOverlayPoint(stroke.points_norm[i], rect));
    }
    painter->drawPath(path);
  };

  for (const InkStroke& stroke : ink_strokes_) {
    draw_stroke(stroke);
  }
  if (has_active_stroke_) {
    draw_stroke(active_stroke_);
  }
  painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
}

QPointF PdfTabView::NormalizeOverlayPoint(const QPointF& pos, const QRect& rect) const {
  if (rect.width() <= 0 || rect.height() <= 0) {
    return QPointF(0.0, 0.0);
  }
  const qreal nx = std::clamp((pos.x() - rect.left()) / static_cast<qreal>(rect.width()), 0.0, 1.0);
  const qreal ny = std::clamp((pos.y() - rect.top()) / static_cast<qreal>(rect.height()), 0.0, 1.0);
  return QPointF(nx, ny);
}

QPointF PdfTabView::DenormalizeOverlayPoint(const QPointF& normalized, const QRect& rect) const {
  return QPointF(rect.left() + normalized.x() * rect.width(),
                 rect.top() + normalized.y() * rect.height());
}

bool PdfTabView::TryEraseNearestStroke(const QPointF& pos, const QRect& rect) {
  const qreal threshold_px = 18.0;
  const qreal threshold_sq = threshold_px * threshold_px;

  for (int i = ink_strokes_.size() - 1; i >= 0; --i) {
    const InkStroke& stroke = ink_strokes_[i];
    if (stroke.page != current_page_) {
      continue;
    }
    for (const QPointF& pt_norm : stroke.points_norm) {
      const QPointF pt = DenormalizeOverlayPoint(pt_norm, rect);
      const qreal dx = pos.x() - pt.x();
      const qreal dy = pos.y() - pt.y();
      if (dx * dx + dy * dy <= threshold_sq) {
        ink_strokes_.removeAt(i);
        return true;
      }
    }
  }
  return false;
}

}  // namespace qt_pdftools::ui
