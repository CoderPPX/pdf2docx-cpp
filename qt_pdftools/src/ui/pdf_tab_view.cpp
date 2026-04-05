#include "qt_pdftools/ui/pdf_tab_view.hpp"

#include <algorithm>

#include <QFileInfo>
#include <QLabel>
#include <QPointF>
#include <QVBoxLayout>

#if QT_PDFTOOLS_HAS_QTPDF
#include <QtPdf/qpdfdocument.h>
#include <QtPdf/qpdfpagenavigator.h>
#include <QtPdfWidgets/qpdfview.h>
#endif

namespace qt_pdftools::ui {

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

  if (view_->pageNavigator() != nullptr) {
    connect(view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this, [this](int page_index) {
      current_page_ = std::max(1, page_index + 1);
      emit StateChanged();
    });
  }

  connect(view_, &QPdfView::zoomFactorChanged, this, [this](qreal value) {
    zoom_factor_ = std::clamp(value, 0.2, 5.0);
    emit StateChanged();
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
    view_->setDocument(nullptr);
  }
  if (document_ != nullptr) {
    document_->close();
  }
#endif
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
  if (view_->pageNavigator() != nullptr && page_count_ > 0) {
    view_->pageNavigator()->jump(0, QPointF(), zoom_factor_);
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

}  // namespace qt_pdftools::ui
