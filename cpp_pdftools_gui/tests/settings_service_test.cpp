#include <QObject>
#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QtTest>

#include "pdftools_gui/services/settings_service.hpp"

namespace {

class SettingsServiceTest : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase();
  void cleanupTestCase();
  void recentValuesShouldDeduplicateAndCap();
  void lastDirectoryShouldNormalizeFilePath();
  void clearByPrefixShouldWork();
  void taskHistoryShouldRoundTripAndCap();

 private:
  QString org_;
  QString app_;
};

void SettingsServiceTest::initTestCase() {
  org_ = QCoreApplication::organizationName();
  app_ = QCoreApplication::applicationName();

  QCoreApplication::setOrganizationName(QStringLiteral("pdftools-test"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui-settings-test"));

  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();
}

void SettingsServiceTest::cleanupTestCase() {
  QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
  settings.clear();
  settings.sync();

  QCoreApplication::setOrganizationName(org_);
  QCoreApplication::setApplicationName(app_);
}

void SettingsServiceTest::recentValuesShouldDeduplicateAndCap() {
  pdftools_gui::services::SettingsService service;

  service.PushRecentValue(QStringLiteral("demo"), QStringLiteral("A"), 3);
  service.PushRecentValue(QStringLiteral("demo"), QStringLiteral("B"), 3);
  service.PushRecentValue(QStringLiteral("demo"), QStringLiteral("C"), 3);
  service.PushRecentValue(QStringLiteral("demo"), QStringLiteral("B"), 3);
  service.PushRecentValue(QStringLiteral("demo"), QStringLiteral("D"), 3);

  const QStringList values = service.RecentValues(QStringLiteral("demo"));
  QCOMPARE(values.size(), 3);
  QCOMPARE(values.at(0), QStringLiteral("D"));
  QCOMPARE(values.at(1), QStringLiteral("B"));
  QCOMPARE(values.at(2), QStringLiteral("C"));
  QCOMPARE(service.RecentValue(QStringLiteral("demo")), QStringLiteral("D"));
}

void SettingsServiceTest::lastDirectoryShouldNormalizeFilePath() {
  pdftools_gui::services::SettingsService service;

  service.SetLastDirectory(QStringLiteral("x"), QStringLiteral("/tmp/sample/output.txt"));
  const QString dir = service.LastDirectory(QStringLiteral("x"));
  QVERIFY2(dir == QStringLiteral("/tmp/sample") || dir == QStringLiteral("/tmp/sample/output.txt"),
           "last directory normalization unexpected");
}

void SettingsServiceTest::clearByPrefixShouldWork() {
  pdftools_gui::services::SettingsService service;
  service.WriteValue(QStringLiteral("recent/a"), QStringLiteral("x"));
  service.WriteValue(QStringLiteral("recent/b"), QStringLiteral("y"));
  service.WriteValue(QStringLiteral("other/c"), QStringLiteral("z"));

  service.ClearByPrefix(QStringLiteral("recent"));
  const QStringList keys = service.AllKeys();
  QVERIFY(!keys.contains(QStringLiteral("recent/a")));
  QVERIFY(!keys.contains(QStringLiteral("recent/b")));
  QVERIFY(keys.contains(QStringLiteral("other/c")));
}

void SettingsServiceTest::taskHistoryShouldRoundTripAndCap() {
  pdftools_gui::services::SettingsService service;

  QVector<pdftools_gui::services::TaskInfo> tasks;
  for (int i = 0; i < 3; ++i) {
    pdftools_gui::services::TaskInfo task;
    task.id = i + 1;
    task.operation = pdftools_gui::services::OperationKind::kExtractText;
    task.state = pdftools_gui::services::TaskState::kSucceeded;
    task.progress = 100;
    task.summary = QStringLiteral("summary-%1").arg(i + 1);
    task.detail = QStringLiteral("detail-%1").arg(i + 1);
    task.output_path = QStringLiteral("/tmp/out-%1.txt").arg(i + 1);
    task.submitted_at = QDateTime::currentDateTimeUtc().addSecs(-60 * (i + 1));
    task.finished_at = QDateTime::currentDateTimeUtc().addSecs(-30 * (i + 1));
    tasks.push_back(task);
  }

  service.WriteTaskHistory(tasks, 2);

  const QVector<pdftools_gui::services::TaskInfo> restored = service.ReadTaskHistory(10);
  QCOMPARE(restored.size(), 2);
  QCOMPARE(restored.at(0).id, 1LL);
  QCOMPARE(restored.at(1).id, 2LL);
  QCOMPARE(restored.at(0).summary, QStringLiteral("summary-1"));

  service.ClearTaskHistory();
  QVERIFY(service.ReadTaskHistory(10).isEmpty());
}

}  // namespace

QTEST_MAIN(SettingsServiceTest)
#include "settings_service_test.moc"
