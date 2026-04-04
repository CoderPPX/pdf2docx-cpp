#include <memory>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextBrowser>
#include <QtTest>

#include "pdftools/pdf/document_ops.hpp"
#include "pdftools_gui/pages/merge_page.hpp"
#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/file_list_editor.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_PDF_PATH
#define PDFTOOLS_GUI_TEST_PDF_PATH ""
#endif

#ifndef PDFTOOLS_GUI_TEST_ALT_PDF_PATH
#define PDFTOOLS_GUI_TEST_ALT_PDF_PATH ""
#endif

class MergePageTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void cleanupTestCase();
  void summaryShouldShowTotalPagesForReadableInputs();
  void summaryShouldShowUnreadableCount();
  void previewShouldRenderSelectedPdfPage();

 private:
  QString org_;
  QString app_;
};

void MergePageTest::initTestCase() {
  org_ = QCoreApplication::organizationName();
  app_ = QCoreApplication::applicationName();

  QCoreApplication::setOrganizationName(QStringLiteral("pdftools-test"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui-merge-page-test"));

  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();
}

void MergePageTest::cleanupTestCase() {
  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();

  QCoreApplication::setOrganizationName(org_);
  QCoreApplication::setApplicationName(app_);
}

void MergePageTest::summaryShouldShowTotalPagesForReadableInputs() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  const QString alt = QString::fromUtf8(PDFTOOLS_GUI_TEST_ALT_PDF_PATH);
  if (fixture.isEmpty() || alt.isEmpty() || !QFileInfo::exists(fixture) || !QFileInfo::exists(alt)) {
    QSKIP("fixture PDFs not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::MergePage page(manager.get(), settings.get());
  page.show();

  auto* input_editor =
      page.findChild<pdftools_gui::widgets::FileListEditor*>(QStringLiteral("merge_input_files"));
  auto* summary = page.findChild<QLabel*>(QStringLiteral("merge_input_summary_label"));
  QVERIFY(input_editor != nullptr);
  QVERIFY(summary != nullptr);

  pdftools::pdf::PdfInfoResult fixture_info;
  pdftools::pdf::PdfInfoRequest request;
  request.input_pdf = fixture.toStdString();
  QVERIFY(pdftools::pdf::GetPdfInfo(request, &fixture_info).ok());
  pdftools::pdf::PdfInfoResult alt_info;
  request.input_pdf = alt.toStdString();
  QVERIFY(pdftools::pdf::GetPdfInfo(request, &alt_info).ok());

  input_editor->SetPaths(QStringList{fixture, alt});
  QCoreApplication::processEvents();

  const uint32_t total = fixture_info.page_count + alt_info.page_count;
  QVERIFY(summary->text().contains(QStringLiteral("已选择 2 个文件")));
  QVERIFY(summary->text().contains(QString::number(total)));
}

void MergePageTest::summaryShouldShowUnreadableCount() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::MergePage page(manager.get(), settings.get());
  page.show();

  auto* input_editor =
      page.findChild<pdftools_gui::widgets::FileListEditor*>(QStringLiteral("merge_input_files"));
  auto* summary = page.findChild<QLabel*>(QStringLiteral("merge_input_summary_label"));
  QVERIFY(input_editor != nullptr);
  QVERIFY(summary != nullptr);

  input_editor->SetPaths(
      QStringList{fixture, QStringLiteral("/tmp/pdftools_missing_merge_info_fixture.pdf")});
  QCoreApplication::processEvents();

  QVERIFY(summary->text().contains(QStringLiteral("已选择 2 个文件")));
  QVERIFY(summary->text().contains(QStringLiteral("无法读取 1 个")));
}

void MergePageTest::previewShouldRenderSelectedPdfPage() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::MergePage page(manager.get(), settings.get());
  page.show();

  auto* input_editor =
      page.findChild<pdftools_gui::widgets::FileListEditor*>(QStringLiteral("merge_input_files"));
  auto* preview_page_spin = page.findChild<QSpinBox*>(QStringLiteral("merge_preview_page_spin"));
  auto* refresh_button = page.findChild<QPushButton*>(QStringLiteral("merge_preview_refresh_button"));
  auto* preview_summary = page.findChild<QLabel*>(QStringLiteral("merge_preview_summary_label"));
  auto* preview_browser = page.findChild<QTextBrowser*>(QStringLiteral("merge_preview_browser"));
  QVERIFY(input_editor != nullptr);
  QVERIFY(preview_page_spin != nullptr);
  QVERIFY(refresh_button != nullptr);
  QVERIFY(preview_summary != nullptr);
  QVERIFY(preview_browser != nullptr);

  input_editor->SetPaths(QStringList{fixture});
  QCoreApplication::processEvents();
  QVERIFY(preview_page_spin->maximum() >= 1);

  QTest::mouseClick(refresh_button, Qt::LeftButton);
  QCoreApplication::processEvents();

  QVERIFY(preview_summary->text().contains(QStringLiteral("预览完成")));
  QVERIFY(preview_browser->toHtml().contains(QStringLiteral("Page")));
}

}  // namespace

QTEST_MAIN(MergePageTest)
#include "merge_page_test.moc"
