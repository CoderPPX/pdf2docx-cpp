#include <QObject>
#include <QtTest>

#include "pdftools_gui/models/task_item_model.hpp"

namespace {

class TaskItemModelTest : public QObject {
  Q_OBJECT

 private slots:
  void upsertShouldInsertAndUpdate();
  void snapshotAndReplaceShouldPreserveOrdering();
  void removeFinalTasksShouldKeepActiveOnly();
};

void TaskItemModelTest::upsertShouldInsertAndUpdate() {
  pdftools_gui::models::TaskItemModel model;

  pdftools_gui::services::TaskInfo t1;
  t1.id = 1;
  t1.operation = pdftools_gui::services::OperationKind::kMergePdf;
  t1.state = pdftools_gui::services::TaskState::kRunning;
  t1.progress = 20;
  t1.summary = QStringLiteral("running");

  model.UpsertTask(t1);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.data(model.index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 1LL);

  t1.state = pdftools_gui::services::TaskState::kSucceeded;
  t1.progress = 100;
  t1.summary = QStringLiteral("done");
  model.UpsertTask(t1);

  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.data(model.index(0, pdftools_gui::models::TaskItemModel::kStatus)).toString(),
           QStringLiteral("Succeeded"));

  pdftools_gui::services::TaskInfo t2;
  t2.id = 2;
  t2.operation = pdftools_gui::services::OperationKind::kExtractText;
  t2.state = pdftools_gui::services::TaskState::kFailed;
  t2.progress = 100;
  t2.summary = QStringLiteral("failed");

  model.UpsertTask(t2);
  QCOMPARE(model.rowCount(), 2);
  QCOMPARE(model.data(model.index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 2LL);
  QCOMPARE(model.data(model.index(1, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 1LL);
}

void TaskItemModelTest::snapshotAndReplaceShouldPreserveOrdering() {
  pdftools_gui::models::TaskItemModel model;

  pdftools_gui::services::TaskInfo t1;
  t1.id = 101;
  t1.state = pdftools_gui::services::TaskState::kSucceeded;
  t1.summary = QStringLiteral("t1");
  model.UpsertTask(t1);

  pdftools_gui::services::TaskInfo t2;
  t2.id = 102;
  t2.state = pdftools_gui::services::TaskState::kFailed;
  t2.summary = QStringLiteral("t2");
  model.UpsertTask(t2);

  const QVector<pdftools_gui::services::TaskInfo> snapshot = model.SnapshotTasks();
  QCOMPARE(snapshot.size(), 2);
  QCOMPARE(snapshot.at(0).id, 102LL);
  QCOMPARE(snapshot.at(1).id, 101LL);

  pdftools_gui::models::TaskItemModel restored;
  restored.ReplaceAllTasks(snapshot);
  QCOMPARE(restored.rowCount(), 2);
  QCOMPARE(restored.data(restored.index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 102LL);
  QCOMPARE(restored.data(restored.index(1, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 101LL);
}

void TaskItemModelTest::removeFinalTasksShouldKeepActiveOnly() {
  pdftools_gui::models::TaskItemModel model;

  pdftools_gui::services::TaskInfo queued;
  queued.id = 1;
  queued.state = pdftools_gui::services::TaskState::kQueued;
  model.UpsertTask(queued);

  pdftools_gui::services::TaskInfo running;
  running.id = 2;
  running.state = pdftools_gui::services::TaskState::kRunning;
  model.UpsertTask(running);

  pdftools_gui::services::TaskInfo done;
  done.id = 3;
  done.state = pdftools_gui::services::TaskState::kSucceeded;
  model.UpsertTask(done);

  pdftools_gui::services::TaskInfo canceled;
  canceled.id = 4;
  canceled.state = pdftools_gui::services::TaskState::kCanceled;
  model.UpsertTask(canceled);

  model.RemoveFinalTasks();
  QCOMPARE(model.rowCount(), 2);
  QCOMPARE(model.data(model.index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 2LL);
  QCOMPARE(model.data(model.index(1, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 1LL);
}

}  // namespace

QTEST_MAIN(TaskItemModelTest)
#include "task_item_model_test.moc"
