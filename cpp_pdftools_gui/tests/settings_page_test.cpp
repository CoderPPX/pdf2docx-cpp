#include <memory>

#include <QCoreApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QSettings>
#include <QSpinBox>
#include <QtTest>

#include "pdftools_gui/pages/settings_page.hpp"
#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/settings_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"
#include "pdftools_gui/widgets/path_picker.hpp"

namespace {

class SettingsPageTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void cleanupTestCase();
  void applyTaskRetentionShouldPersistAndAffectManager();
  void clearRecentShouldRemoveRecentKeys();
  void clearTaskHistoryShouldRemovePersistedTasks();
  void clearTaskHistoryShouldEmitSignal();
  void applyShouldPersistAndEmitReportDirectory();

 private:
  QString org_;
  QString app_;
};

void SettingsPageTest::initTestCase() {
  org_ = QCoreApplication::organizationName();
  app_ = QCoreApplication::applicationName();

  QCoreApplication::setOrganizationName(QStringLiteral("pdftools-test"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui-settings-page-test"));

  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();
}

void SettingsPageTest::cleanupTestCase() {
  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();

  QCoreApplication::setOrganizationName(org_);
  QCoreApplication::setApplicationName(app_);
}

void SettingsPageTest::applyTaskRetentionShouldPersistAndAffectManager() {
  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service, nullptr, 200);

  pdftools_gui::pages::SettingsPage page(manager.get(), settings.get());
  page.show();

  auto* spin = page.findChild<QSpinBox*>(QStringLiteral("max_completed_tasks_spin"));
  auto* concurrent_spin = page.findChild<QSpinBox*>(QStringLiteral("max_concurrent_tasks_spin"));
  auto* report_picker = page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("report_directory_picker"));
  auto* apply_button = page.findChild<QPushButton*>(QStringLiteral("apply_task_retention_button"));
  QVERIFY(spin != nullptr);
  QVERIFY(concurrent_spin != nullptr);
  QVERIFY(report_picker != nullptr);
  QVERIFY(apply_button != nullptr);

  spin->setValue(123);
  concurrent_spin->setValue(3);
  report_picker->SetPath(QStringLiteral("/tmp/pdftools_reports"));
  QTest::mouseClick(apply_button, Qt::LeftButton);

  QCOMPARE(manager->MaxCompletedTasks(), 123);
  QCOMPARE(manager->MaxConcurrentTasks(), 3);
  QCOMPARE(settings->ReadValue(QStringLiteral("settings/max_completed_tasks"), 0).toInt(), 123);
  QCOMPARE(settings->ReadValue(QStringLiteral("settings/max_concurrent_tasks"), 0).toInt(), 3);
  QCOMPARE(settings->ReadValue(QStringLiteral("settings/report_directory"), QString()).toString(),
           QStringLiteral("/tmp/pdftools_reports"));
}

void SettingsPageTest::clearRecentShouldRemoveRecentKeys() {
  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service, nullptr, 200);

  settings->WriteValue(QStringLiteral("recent/a"), QStringLiteral("x"));
  settings->WriteValue(QStringLiteral("recent/b"), QStringLiteral("y"));
  settings->WriteValue(QStringLiteral("other/c"), QStringLiteral("z"));

  pdftools_gui::pages::SettingsPage page(manager.get(), settings.get());
  page.show();

  auto* clear_button = page.findChild<QPushButton*>(QStringLiteral("clear_recent_history_button"));
  QVERIFY(clear_button != nullptr);
  QTest::mouseClick(clear_button, Qt::LeftButton);

  const QStringList keys = settings->AllKeys();
  QVERIFY(!keys.contains(QStringLiteral("recent/a")));
  QVERIFY(!keys.contains(QStringLiteral("recent/b")));
  QVERIFY(keys.contains(QStringLiteral("other/c")));
}

void SettingsPageTest::clearTaskHistoryShouldRemovePersistedTasks() {
  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service, nullptr, 200);

  pdftools_gui::services::TaskInfo task;
  task.id = 7;
  task.summary = QStringLiteral("failed task");
  task.state = pdftools_gui::services::TaskState::kFailed;
  settings->WriteTaskHistory(QVector<pdftools_gui::services::TaskInfo>{task}, 10);
  QCOMPARE(settings->ReadTaskHistory(10).size(), 1);

  pdftools_gui::pages::SettingsPage page(manager.get(), settings.get());
  page.show();

  auto* clear_button = page.findChild<QPushButton*>(QStringLiteral("clear_task_history_button"));
  QVERIFY(clear_button != nullptr);
  QTest::mouseClick(clear_button, Qt::LeftButton);

  QVERIFY(settings->ReadTaskHistory(10).isEmpty());
}

void SettingsPageTest::clearTaskHistoryShouldEmitSignal() {
  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service, nullptr, 200);

  pdftools_gui::pages::SettingsPage page(manager.get(), settings.get());
  page.show();

  QSignalSpy history_cleared_spy(&page, &pdftools_gui::pages::SettingsPage::TaskHistoryCleared);
  auto* clear_button = page.findChild<QPushButton*>(QStringLiteral("clear_task_history_button"));
  QVERIFY(clear_button != nullptr);

  QTest::mouseClick(clear_button, Qt::LeftButton);
  QCOMPARE(history_cleared_spy.count(), 1);
}

void SettingsPageTest::applyShouldPersistAndEmitReportDirectory() {
  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  auto settings = std::make_unique<pdftools_gui::services::SettingsService>();
  auto manager = std::make_unique<pdftools_gui::services::TaskManager>(service, nullptr, 200);

  pdftools_gui::pages::SettingsPage page(manager.get(), settings.get());
  page.show();

  QSignalSpy report_dir_spy(&page, &pdftools_gui::pages::SettingsPage::ReportDirectoryChanged);
  auto* report_picker = page.findChild<pdftools_gui::widgets::PathPicker*>(QStringLiteral("report_directory_picker"));
  auto* apply_button = page.findChild<QPushButton*>(QStringLiteral("apply_task_retention_button"));
  QVERIFY(report_picker != nullptr);
  QVERIFY(apply_button != nullptr);

  report_picker->SetPath(QStringLiteral("/tmp/reports_v2"));
  QTest::mouseClick(apply_button, Qt::LeftButton);

  QCOMPARE(report_dir_spy.count(), 1);
  const QList<QVariant> args = report_dir_spy.takeFirst();
  QVERIFY(args.size() == 1);
  QCOMPARE(args.at(0).toString(), QStringLiteral("/tmp/reports_v2"));
  QCOMPARE(settings->ReadValue(QStringLiteral("settings/report_directory"), QString()).toString(),
           QStringLiteral("/tmp/reports_v2"));
}

}  // namespace

QTEST_MAIN(SettingsPageTest)
#include "settings_page_test.moc"
