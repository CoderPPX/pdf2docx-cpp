#include <memory>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QtTest>

#include "pdftools/pdf/document_ops.hpp"
#include "pdftools_gui/pages/page_edit_page.hpp"
#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_PDF_PATH
#define PDFTOOLS_GUI_TEST_PDF_PATH ""
#endif

class PageEditPageTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void cleanupTestCase();
  void inputInfoShouldShowPageCountForValidPdf();
  void inputInfoShouldShowErrorForMissingPdf();
  void sourceInfoShouldShowPageCountForValidPdf();
  void spinRangesShouldFollowDetectedPageCount();

 private:
  QString org_;
  QString app_;
};

void PageEditPageTest::initTestCase() {
  org_ = QCoreApplication::organizationName();
  app_ = QCoreApplication::applicationName();

  QCoreApplication::setOrganizationName(QStringLiteral("pdftools-test"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui-page-edit-test"));

  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();
}

void PageEditPageTest::cleanupTestCase() {
  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();

  QCoreApplication::setOrganizationName(org_);
  QCoreApplication::setApplicationName(app_);
}

void PageEditPageTest::inputInfoShouldShowPageCountForValidPdf() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::PageEditPage page(manager.get(), settings.get());
  page.show();

  auto* input_picker =
      page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("page_edit_input_picker"));
  auto* input_label = page.findChild<QLabel*>(QStringLiteral("page_edit_input_info_label"));
  QVERIFY(input_picker != nullptr);
  QVERIFY(input_label != nullptr);

  pdftools::pdf::PdfInfoRequest request;
  request.input_pdf = fixture.toStdString();
  pdftools::pdf::PdfInfoResult result;
  QVERIFY(pdftools::pdf::GetPdfInfo(request, &result).ok());

  input_picker->SetPath(fixture);
  QCoreApplication::processEvents();

  QVERIFY(input_label->text().contains(QString::number(result.page_count)));
}

void PageEditPageTest::inputInfoShouldShowErrorForMissingPdf() {
  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::PageEditPage page(manager.get(), settings.get());
  page.show();

  auto* input_picker =
      page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("page_edit_input_picker"));
  auto* input_label = page.findChild<QLabel*>(QStringLiteral("page_edit_input_info_label"));
  QVERIFY(input_picker != nullptr);
  QVERIFY(input_label != nullptr);

  input_picker->SetPath(QStringLiteral("/tmp/pdftools_missing_page_edit_info_fixture.pdf"));
  QCoreApplication::processEvents();
  QVERIFY(input_label->text().contains(QStringLiteral("无法读取")));
}

void PageEditPageTest::sourceInfoShouldShowPageCountForValidPdf() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::PageEditPage page(manager.get(), settings.get());
  page.show();

  auto* source_picker =
      page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("page_edit_insert_source_picker"));
  auto* source_label = page.findChild<QLabel*>(QStringLiteral("page_edit_insert_source_info_label"));
  QVERIFY(source_picker != nullptr);
  QVERIFY(source_label != nullptr);

  pdftools::pdf::PdfInfoRequest request;
  request.input_pdf = fixture.toStdString();
  pdftools::pdf::PdfInfoResult result;
  QVERIFY(pdftools::pdf::GetPdfInfo(request, &result).ok());

  source_picker->SetPath(fixture);
  QCoreApplication::processEvents();

  QVERIFY(source_label->text().contains(QString::number(result.page_count)));
}

void PageEditPageTest::spinRangesShouldFollowDetectedPageCount() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::PageEditPage page(manager.get(), settings.get());
  page.show();

  pdftools::pdf::PdfInfoRequest request;
  request.input_pdf = fixture.toStdString();
  pdftools::pdf::PdfInfoResult result;
  QVERIFY(pdftools::pdf::GetPdfInfo(request, &result).ok());

  auto* input_picker =
      page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("page_edit_input_picker"));
  auto* delete_spin = page.findChild<QSpinBox*>(QStringLiteral("page_edit_delete_page_spin"));
  auto* insert_at_spin = page.findChild<QSpinBox*>(QStringLiteral("page_edit_insert_at_spin"));
  auto* replace_spin = page.findChild<QSpinBox*>(QStringLiteral("page_edit_replace_page_spin"));
  auto* source_picker =
      page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("page_edit_insert_source_picker"));
  auto* source_page_spin = page.findChild<QSpinBox*>(QStringLiteral("page_edit_insert_source_page_spin"));
  QVERIFY(input_picker != nullptr);
  QVERIFY(delete_spin != nullptr);
  QVERIFY(insert_at_spin != nullptr);
  QVERIFY(replace_spin != nullptr);
  QVERIFY(source_picker != nullptr);
  QVERIFY(source_page_spin != nullptr);

  input_picker->SetPath(fixture);
  source_picker->SetPath(fixture);
  QCoreApplication::processEvents();

  QCOMPARE(delete_spin->maximum(), static_cast<int>(result.page_count));
  QCOMPARE(replace_spin->maximum(), static_cast<int>(result.page_count));
  QCOMPARE(insert_at_spin->maximum(), static_cast<int>(result.page_count + 1));
  QCOMPARE(source_page_spin->maximum(), static_cast<int>(result.page_count));
}

}  // namespace

QTEST_MAIN(PageEditPageTest)
#include "page_edit_page_test.moc"
