#include <filesystem>

#include <QFileInfo>
#include <QObject>
#include <QTemporaryDir>
#include <QtTest>

#include "pdftools_gui/services/pdftools_service.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_PDF_PATH
#define PDFTOOLS_GUI_TEST_PDF_PATH ""
#endif

#ifndef PDFTOOLS_GUI_TEST_ALT_PDF_PATH
#define PDFTOOLS_GUI_TEST_ALT_PDF_PATH ""
#endif

#ifndef PDFTOOLS_GUI_TEST_IMAGE_PATH
#define PDFTOOLS_GUI_TEST_IMAGE_PATH ""
#endif

class PdfToolsServiceTest : public QObject {
  Q_OBJECT

 private slots:
  void extractTextShouldSucceed();
  void mergeShouldSucceed();
  void pageEditShouldSucceed();
  void attachmentsShouldSucceed();
  void image2PdfShouldSucceed();
  void pdf2DocxShouldSucceed();
};

void PdfToolsServiceTest::extractTextShouldSucceed() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools_gui::services::PdfToolsService service;
  pdftools::pdf::ExtractTextRequest request;
  request.input_pdf = fixture.toStdString();
  request.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / "extract.json").string();
  request.output_format = pdftools::pdf::TextOutputFormat::kJson;
  request.page_start = 1;
  request.page_end = 1;
  request.best_effort = true;
  request.overwrite = true;

  const auto outcome = service.Execute(
      pdftools_gui::services::OperationKind::kExtractText,
      pdftools_gui::services::RequestVariant{request});

  QVERIFY2(outcome.success, qPrintable(outcome.detail));
  QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_path)), "extract output missing");
  QVERIFY2(outcome.summary.contains(QStringLiteral("提取成功")), "unexpected summary");
}

void PdfToolsServiceTest::mergeShouldSucceed() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools_gui::services::PdfToolsService service;
  pdftools::pdf::MergePdfRequest request;
  request.input_pdfs = {fixture.toStdString(), fixture.toStdString()};
  request.output_pdf = (std::filesystem::path(temp_dir.path().toStdString()) / "merged.pdf").string();
  request.overwrite = true;

  const auto outcome = service.Execute(
      pdftools_gui::services::OperationKind::kMergePdf,
      pdftools_gui::services::RequestVariant{request});

  QVERIFY2(outcome.success, qPrintable(outcome.detail));
  QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_pdf)), "merged output missing");
  QVERIFY2(outcome.summary.contains(QStringLiteral("合并成功")), "unexpected summary");
}

void PdfToolsServiceTest::pageEditShouldSucceed() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  const QString alt_fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_ALT_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture) || alt_fixture.isEmpty() || !QFileInfo::exists(alt_fixture)) {
    QSKIP("fixture PDFs not found");
  }

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");
  pdftools_gui::services::PdfToolsService service;

  {
    pdftools::pdf::DeletePageRequest request;
    request.input_pdf = alt_fixture.toStdString();
    request.output_pdf = (std::filesystem::path(temp_dir.path().toStdString()) / "delete.pdf").string();
    request.page_number = 1;
    request.overwrite = true;
    const auto outcome = service.Execute(pdftools_gui::services::OperationKind::kDeletePage,
                                         pdftools_gui::services::RequestVariant{request});
    QVERIFY2(outcome.success, qPrintable(outcome.detail));
    QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_pdf)), "delete output missing");
  }

  {
    pdftools::pdf::InsertPageRequest request;
    request.input_pdf = fixture.toStdString();
    request.output_pdf = (std::filesystem::path(temp_dir.path().toStdString()) / "insert.pdf").string();
    request.insert_at = 1;
    request.source_pdf = alt_fixture.toStdString();
    request.source_page_number = 1;
    request.overwrite = true;
    const auto outcome = service.Execute(pdftools_gui::services::OperationKind::kInsertPage,
                                         pdftools_gui::services::RequestVariant{request});
    QVERIFY2(outcome.success, qPrintable(outcome.detail));
    QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_pdf)), "insert output missing");
  }

  {
    pdftools::pdf::ReplacePageRequest request;
    request.input_pdf = alt_fixture.toStdString();
    request.output_pdf = (std::filesystem::path(temp_dir.path().toStdString()) / "replace.pdf").string();
    request.page_number = 1;
    request.source_pdf = fixture.toStdString();
    request.source_page_number = 1;
    request.overwrite = true;
    const auto outcome = service.Execute(pdftools_gui::services::OperationKind::kReplacePage,
                                         pdftools_gui::services::RequestVariant{request});
    QVERIFY2(outcome.success, qPrintable(outcome.detail));
    QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_pdf)), "replace output missing");
  }
}

void PdfToolsServiceTest::attachmentsShouldSucceed() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools_gui::services::PdfToolsService service;
  pdftools::pdf::ExtractAttachmentsRequest request;
  request.input_pdf = fixture.toStdString();
  request.output_dir = (std::filesystem::path(temp_dir.path().toStdString()) / "attachments").string();
  request.best_effort = true;
  request.overwrite = true;

  const auto outcome = service.Execute(pdftools_gui::services::OperationKind::kExtractAttachments,
                                       pdftools_gui::services::RequestVariant{request});
  QVERIFY2(outcome.success, qPrintable(outcome.detail));
  QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_dir)), "attachments output dir missing");
}

void PdfToolsServiceTest::image2PdfShouldSucceed() {
  const QString image_path = QString::fromUtf8(PDFTOOLS_GUI_TEST_IMAGE_PATH);
  if (image_path.isEmpty() || !QFileInfo::exists(image_path)) {
    QSKIP("test image not found");
  }

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools_gui::services::PdfToolsService service;
  pdftools::pdf::ImagesToPdfRequest request;
  request.image_paths = {image_path.toStdString(), image_path.toStdString()};
  request.output_pdf = (std::filesystem::path(temp_dir.path().toStdString()) / "images.pdf").string();
  request.overwrite = true;
  request.scale_mode = pdftools::pdf::ImageScaleMode::kFit;

  const auto outcome = service.Execute(pdftools_gui::services::OperationKind::kImagesToPdf,
                                       pdftools_gui::services::RequestVariant{request});
  QVERIFY2(outcome.success, qPrintable(outcome.detail));
  QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_pdf)), "image2pdf output missing");
}

void PdfToolsServiceTest::pdf2DocxShouldSucceed() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools_gui::services::PdfToolsService service;
  pdftools::convert::PdfToDocxRequest request;
  request.input_pdf = fixture.toStdString();
  request.output_docx = (std::filesystem::path(temp_dir.path().toStdString()) / "out.docx").string();
  request.dump_ir_json_path = (std::filesystem::path(temp_dir.path().toStdString()) / "dump.json").string();
  request.extract_images = true;
  request.enable_font_fallback = true;
  request.overwrite = true;

  const auto outcome = service.Execute(pdftools_gui::services::OperationKind::kPdfToDocx,
                                       pdftools_gui::services::RequestVariant{request});
  QVERIFY2(outcome.success, qPrintable(outcome.detail));
  QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_docx)), "pdf2docx output missing");
  QVERIFY2(QFileInfo::exists(QString::fromStdString(request.dump_ir_json_path)), "dump ir output missing");
}

}  // namespace

QTEST_MAIN(PdfToolsServiceTest)
#include "pdftools_service_test.moc"
