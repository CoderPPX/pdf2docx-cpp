#include <memory>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLabel>
#include <QSettings>
#include <QtTest>

#include "pdftools_gui/pages/image_to_pdf_page.hpp"
#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/file_list_editor.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_IMAGE_PATH
#define PDFTOOLS_GUI_TEST_IMAGE_PATH ""
#endif

class ImageToPdfPageTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void cleanupTestCase();
  void imagePreviewSummaryShouldReflectReadableAndUnreadableInputs();

 private:
  QString org_;
  QString app_;
};

void ImageToPdfPageTest::initTestCase() {
  org_ = QCoreApplication::organizationName();
  app_ = QCoreApplication::applicationName();

  QCoreApplication::setOrganizationName(QStringLiteral("pdftools-test"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui-image-to-pdf-page-test"));

  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();
}

void ImageToPdfPageTest::cleanupTestCase() {
  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();

  QCoreApplication::setOrganizationName(org_);
  QCoreApplication::setApplicationName(app_);
}

void ImageToPdfPageTest::imagePreviewSummaryShouldReflectReadableAndUnreadableInputs() {
  const QString fixture_image = QString::fromUtf8(PDFTOOLS_GUI_TEST_IMAGE_PATH);
  if (fixture_image.isEmpty() || !QFileInfo::exists(fixture_image)) {
    QSKIP("fixture image not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service);
  pdftools_gui::pages::ImageToPdfPage page(manager.get(), settings.get());
  page.show();

  auto* editor =
      page.findChild<pdftools_gui::widgets::FileListEditor*>(QStringLiteral("image2pdf_input_files"));
  auto* summary = page.findChild<QLabel*>(QStringLiteral("image2pdf_preview_summary_label"));
  QVERIFY(editor != nullptr);
  QVERIFY(summary != nullptr);

  editor->SetPaths(QStringList{fixture_image, QStringLiteral("/tmp/pdftools_missing_image_for_preview.png")});
  QCoreApplication::processEvents();

  QVERIFY(summary->text().contains(QStringLiteral("1 张可读图片")));
  QVERIFY(summary->text().contains(QStringLiteral("1 张无法读取")));
}

}  // namespace

QTEST_MAIN(ImageToPdfPageTest)
#include "image_to_pdf_page_test.moc"
