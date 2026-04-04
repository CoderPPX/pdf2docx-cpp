#include <memory>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextBrowser>
#include <QtTest>

#include "pdftools_gui/pages/pdf2docx_page.hpp"
#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_PDF_PATH
#define PDFTOOLS_GUI_TEST_PDF_PATH ""
#endif

class Pdf2DocxPageTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void cleanupTestCase();
  void irPreviewShouldUpdateForValidPdf();

 private:
  QString org_;
  QString app_;
};

void Pdf2DocxPageTest::initTestCase() {
  org_ = QCoreApplication::organizationName();
  app_ = QCoreApplication::applicationName();

  QCoreApplication::setOrganizationName(QStringLiteral("pdftools-test"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui-pdf2docx-page-test"));

  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();
}

void Pdf2DocxPageTest::cleanupTestCase() {
  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();

  QCoreApplication::setOrganizationName(org_);
  QCoreApplication::setApplicationName(app_);
}

void Pdf2DocxPageTest::irPreviewShouldUpdateForValidPdf() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::Pdf2DocxPage page(manager.get(), settings.get());
  page.show();

  auto* input_picker =
      page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("pdf2docx_input_picker"));
  auto* input_info = page.findChild<QLabel*>(QStringLiteral("pdf2docx_input_info_label"));
  auto* preview_spin = page.findChild<QSpinBox*>(QStringLiteral("pdf2docx_preview_page_spin"));
  auto* refresh_button = page.findChild<QPushButton*>(QStringLiteral("pdf2docx_preview_refresh_button"));
  auto* preview_summary = page.findChild<QLabel*>(QStringLiteral("pdf2docx_preview_summary_label"));
  auto* preview_browser = page.findChild<QTextBrowser*>(QStringLiteral("pdf2docx_preview_browser"));
  auto* images_list = page.findChild<QListWidget*>(QStringLiteral("pdf2docx_preview_images_list"));
  QVERIFY(input_picker != nullptr);
  QVERIFY(input_info != nullptr);
  QVERIFY(preview_spin != nullptr);
  QVERIFY(refresh_button != nullptr);
  QVERIFY(preview_summary != nullptr);
  QVERIFY(preview_browser != nullptr);
  QVERIFY(images_list != nullptr);

  input_picker->SetPath(fixture);
  QCoreApplication::processEvents();
  QVERIFY(input_info->text().contains(QStringLiteral("页数")));
  QVERIFY(preview_spin->maximum() >= 1);

  QTest::mouseClick(refresh_button, Qt::LeftButton);
  QCoreApplication::processEvents();
  QVERIFY(preview_summary->text().contains(QStringLiteral("预览完成")));
  QVERIFY(preview_browser->toHtml().contains(QStringLiteral("Page")));
  QVERIFY(images_list->count() >= 0);
}

}  // namespace

QTEST_MAIN(Pdf2DocxPageTest)
#include "pdf2docx_page_test.moc"
