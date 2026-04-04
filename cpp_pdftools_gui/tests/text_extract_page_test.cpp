#include <memory>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextBrowser>
#include <QtTest>

#include "pdftools_gui/pages/text_extract_page.hpp"
#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_PDF_PATH
#define PDFTOOLS_GUI_TEST_PDF_PATH ""
#endif

class TextExtractPageTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void cleanupTestCase();
  void inputInfoAndPreviewShouldUpdateForValidPdf();

 private:
  QString org_;
  QString app_;
};

void TextExtractPageTest::initTestCase() {
  org_ = QCoreApplication::organizationName();
  app_ = QCoreApplication::applicationName();

  QCoreApplication::setOrganizationName(QStringLiteral("pdftools-test"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui-text-extract-page-test"));

  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();
}

void TextExtractPageTest::cleanupTestCase() {
  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();

  QCoreApplication::setOrganizationName(org_);
  QCoreApplication::setApplicationName(app_);
}

void TextExtractPageTest::inputInfoAndPreviewShouldUpdateForValidPdf() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::TextExtractPage page(manager.get(), settings.get());
  page.show();

  auto* input_picker =
      page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("text_extract_input_picker"));
  auto* input_info = page.findChild<QLabel*>(QStringLiteral("text_extract_input_info_label"));
  auto* preview_spin = page.findChild<QSpinBox*>(QStringLiteral("text_extract_preview_page_spin"));
  auto* refresh_button = page.findChild<QPushButton*>(QStringLiteral("text_extract_preview_refresh_button"));
  auto* preview_summary = page.findChild<QLabel*>(QStringLiteral("text_extract_preview_summary_label"));
  auto* preview_browser = page.findChild<QTextBrowser*>(QStringLiteral("text_extract_preview_browser"));
  QVERIFY(input_picker != nullptr);
  QVERIFY(input_info != nullptr);
  QVERIFY(preview_spin != nullptr);
  QVERIFY(refresh_button != nullptr);
  QVERIFY(preview_summary != nullptr);
  QVERIFY(preview_browser != nullptr);

  input_picker->SetPath(fixture);
  QCoreApplication::processEvents();
  QVERIFY(input_info->text().contains(QStringLiteral("页数")));
  QVERIFY(preview_spin->maximum() >= 1);

  QTest::mouseClick(refresh_button, Qt::LeftButton);
  QCoreApplication::processEvents();
  QVERIFY(preview_summary->text().contains(QStringLiteral("预览完成")));
  QVERIFY(preview_browser->toHtml().contains(QStringLiteral("Page")));
}

}  // namespace

QTEST_MAIN(TextExtractPageTest)
#include "text_extract_page_test.moc"
