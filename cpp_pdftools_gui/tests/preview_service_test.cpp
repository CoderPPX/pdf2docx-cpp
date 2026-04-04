#include <QObject>
#include <QFileInfo>
#include <QtTest>

#include "pdftools_gui/services/preview_service.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_PDF_PATH
#define PDFTOOLS_GUI_TEST_PDF_PATH ""
#endif

class PreviewServiceTest : public QObject {
  Q_OBJECT

 private slots:
  void queryPdfPageCountShouldReturnPositiveForFixture();
  void renderIrPreviewShouldProduceHtml();
  void extractImageThumbnailsShouldNotFail();
};

void PreviewServiceTest::queryPdfPageCountShouldReturnPositiveForFixture() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  pdftools_gui::services::PreviewService service;
  QString error;
  const int pages = service.QueryPdfPageCount(fixture, &error);
  QVERIFY2(pages > 0, error.toUtf8().constData());
}

void PreviewServiceTest::renderIrPreviewShouldProduceHtml() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  pdftools_gui::services::PreviewService service;
  const auto preview = service.RenderPdfIrPreview(fixture, 1, true, 1.0, 300, 20);
  QVERIFY(preview.success);
  QVERIFY(preview.total_pages > 0);
  QVERIFY(preview.rendered_pages >= 1);
  QVERIFY(preview.html.contains(QStringLiteral("<html")));
  QVERIFY(preview.html.contains(QStringLiteral("Page")));
}

void PreviewServiceTest::extractImageThumbnailsShouldNotFail() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  pdftools_gui::services::PreviewService service;
  QString error;
  const auto thumbs = service.ExtractImageThumbnailsFromPdf(fixture, 0, 96, 16, &error);
  QVERIFY2(error.isEmpty(), error.toUtf8().constData());
  for (const auto& thumb : thumbs) {
    QVERIFY(!thumb.image.isNull());
  }
}

}  // namespace

QTEST_MAIN(PreviewServiceTest)
#include "preview_service_test.moc"
