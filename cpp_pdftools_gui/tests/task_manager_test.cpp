#include <filesystem>
#include <memory>
#include <string>

#include <QFileInfo>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "pdftools_gui/services/pdftools_service.hpp"
#include "pdftools_gui/services/task_manager.hpp"

namespace {

#ifndef PDFTOOLS_GUI_TEST_PDF_PATH
#define PDFTOOLS_GUI_TEST_PDF_PATH ""
#endif

class TaskManagerTest : public QObject {
  Q_OBJECT

 private slots:
  void submitShouldEmitCompleted();
  void submitShouldEmitQueuedState();
  void submitShouldWorkWithNullService();
  void concurrentLimitShouldBeConfigurable();
  void retainedTaskCountShouldRespectMaxLimit();
  void cancelQueuedTaskShouldEmitCanceled();
  void clearRetainedTasksShouldDropCompletedItems();
};

void TaskManagerTest::submitShouldEmitCompleted() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  pdftools_gui::services::TaskManager manager(service);

  QSignalSpy completed_spy(&manager, &pdftools_gui::services::TaskManager::TaskCompleted);

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools::pdf::ExtractTextRequest request;
  request.input_pdf = fixture.toStdString();
  request.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / "text.txt").string();
  request.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
  request.overwrite = true;

  const qint64 task_id = manager.Submit(pdftools_gui::services::OperationKind::kExtractText,
                                        QStringLiteral("Extract Text"),
                                        pdftools_gui::services::RequestVariant{request});
  QVERIFY(task_id > 0);

  QTRY_VERIFY_WITH_TIMEOUT(completed_spy.count() == 1, 20000);

  const QList<QVariant> args = completed_spy.takeFirst();
  QVERIFY(args.size() == 1);

  const auto info = args[0].value<pdftools_gui::services::TaskInfo>();
  QCOMPARE(info.id, task_id);
  QCOMPARE(info.state, pdftools_gui::services::TaskState::kSucceeded);
  QVERIFY2(QFileInfo::exists(QString::fromStdString(request.output_path)), "task output missing");
}

void TaskManagerTest::submitShouldEmitQueuedState() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  pdftools_gui::services::TaskManager manager(service, nullptr, 200, 1);
  QSignalSpy changed_spy(&manager, &pdftools_gui::services::TaskManager::TaskChanged);
  QSignalSpy completed_spy(&manager, &pdftools_gui::services::TaskManager::TaskCompleted);

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools::pdf::ExtractTextRequest request;
  request.input_pdf = fixture.toStdString();
  request.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / "queued.txt").string();
  request.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
  request.overwrite = true;

  const qint64 task_id = manager.Submit(pdftools_gui::services::OperationKind::kExtractText,
                                        QStringLiteral("Extract Text"),
                                        pdftools_gui::services::RequestVariant{request});
  QVERIFY(task_id > 0);

  QTRY_VERIFY_WITH_TIMEOUT(changed_spy.count() >= 1, 5000);
  QTRY_VERIFY_WITH_TIMEOUT(completed_spy.count() == 1, 20000);

  bool seen_queued = false;
  for (int i = 0; i < changed_spy.count(); ++i) {
    const QList<QVariant> args = changed_spy.at(i);
    if (args.size() != 1) {
      continue;
    }
    const auto info = args[0].value<pdftools_gui::services::TaskInfo>();
    if (info.id == task_id && info.state == pdftools_gui::services::TaskState::kQueued) {
      seen_queued = true;
      break;
    }
  }
  QVERIFY(seen_queued);
}

void TaskManagerTest::submitShouldWorkWithNullService() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  pdftools_gui::services::TaskManager manager(nullptr, nullptr, 1);
  QSignalSpy completed_spy(&manager, &pdftools_gui::services::TaskManager::TaskCompleted);

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  for (int i = 0; i < 2; ++i) {
    pdftools::pdf::ExtractTextRequest request;
    request.input_pdf = fixture.toStdString();
    request.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / ("text_" + std::to_string(i) + ".txt")).string();
    request.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
    request.overwrite = true;

    manager.Submit(pdftools_gui::services::OperationKind::kExtractText,
                   QStringLiteral("Extract Text"),
                   pdftools_gui::services::RequestVariant{request});
  }

  QTRY_VERIFY_WITH_TIMEOUT(completed_spy.count() == 2, 20000);
}

void TaskManagerTest::concurrentLimitShouldBeConfigurable() {
  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  pdftools_gui::services::TaskManager manager(service, nullptr, 200, 2);
  QCOMPARE(manager.MaxConcurrentTasks(), 2);
  manager.SetMaxConcurrentTasks(3);
  QCOMPARE(manager.MaxConcurrentTasks(), 3);
}

void TaskManagerTest::retainedTaskCountShouldRespectMaxLimit() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  pdftools_gui::services::TaskManager manager(service, nullptr, 1);
  QSignalSpy completed_spy(&manager, &pdftools_gui::services::TaskManager::TaskCompleted);

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  for (int i = 0; i < 3; ++i) {
    pdftools::pdf::ExtractTextRequest request;
    request.input_pdf = fixture.toStdString();
    request.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / ("cap_" + std::to_string(i) + ".txt")).string();
    request.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
    request.overwrite = true;
    manager.Submit(pdftools_gui::services::OperationKind::kExtractText,
                   QStringLiteral("Extract Text"),
                   pdftools_gui::services::RequestVariant{request});
  }

  QTRY_VERIFY_WITH_TIMEOUT(completed_spy.count() == 3, 20000);
  QTRY_VERIFY_WITH_TIMEOUT(manager.RetainedTaskCount() <= 1, 10000);
}

void TaskManagerTest::cancelQueuedTaskShouldEmitCanceled() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  pdftools_gui::services::TaskManager manager(service, nullptr, 200, 1);
  QSignalSpy completed_spy(&manager, &pdftools_gui::services::TaskManager::TaskCompleted);

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools::pdf::ExtractTextRequest first;
  first.input_pdf = fixture.toStdString();
  first.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / "first.txt").string();
  first.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
  first.overwrite = true;

  pdftools::pdf::ExtractTextRequest second;
  second.input_pdf = fixture.toStdString();
  second.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / "second.txt").string();
  second.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
  second.overwrite = true;

  const qint64 first_id = manager.Submit(pdftools_gui::services::OperationKind::kExtractText,
                                         QStringLiteral("First"),
                                         pdftools_gui::services::RequestVariant{first});
  const qint64 second_id = manager.Submit(pdftools_gui::services::OperationKind::kExtractText,
                                          QStringLiteral("Second"),
                                          pdftools_gui::services::RequestVariant{second});
  QVERIFY(first_id > 0);
  QVERIFY(second_id > 0);
  QVERIFY(manager.CancelTask(second_id));

  QTRY_VERIFY_WITH_TIMEOUT(completed_spy.count() >= 1, 20000);

  bool seen_canceled = false;
  for (int i = 0; i < completed_spy.count(); ++i) {
    const QList<QVariant> args = completed_spy.at(i);
    if (args.size() != 1) {
      continue;
    }
    const auto info = args[0].value<pdftools_gui::services::TaskInfo>();
    if (info.id == second_id && info.state == pdftools_gui::services::TaskState::kCanceled) {
      seen_canceled = true;
      break;
    }
  }
  QVERIFY(seen_canceled);
}

void TaskManagerTest::clearRetainedTasksShouldDropCompletedItems() {
  const QString fixture = QString::fromUtf8(PDFTOOLS_GUI_TEST_PDF_PATH);
  if (fixture.isEmpty() || !QFileInfo::exists(fixture)) {
    QSKIP("fixture PDF not found");
  }

  auto service = std::make_shared<pdftools_gui::services::PdfToolsService>();
  pdftools_gui::services::TaskManager manager(service, nullptr, 200, 1);
  QSignalSpy completed_spy(&manager, &pdftools_gui::services::TaskManager::TaskCompleted);

  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools::pdf::ExtractTextRequest request;
  request.input_pdf = fixture.toStdString();
  request.output_path = (std::filesystem::path(temp_dir.path().toStdString()) / "clear_test.txt").string();
  request.output_format = pdftools::pdf::TextOutputFormat::kPlainText;
  request.overwrite = true;

  manager.Submit(pdftools_gui::services::OperationKind::kExtractText,
                 QStringLiteral("Extract Text"),
                 pdftools_gui::services::RequestVariant{request});
  QTRY_VERIFY_WITH_TIMEOUT(completed_spy.count() == 1, 20000);
  QVERIFY(manager.RetainedTaskCount() >= 1);

  manager.ClearRetainedTasks();
  QCOMPARE(manager.RetainedTaskCount(), 0);
}

}  // namespace

QTEST_MAIN(TaskManagerTest)
#include "task_manager_test.moc"
