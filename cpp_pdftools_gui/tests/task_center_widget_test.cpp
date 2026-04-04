#include <QObject>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>
#include <QtTest>

#include "pdftools_gui/models/task_item_model.hpp"
#include "pdftools_gui/widgets/task_center_widget.hpp"

namespace {

class TaskCenterWidgetTest : public QObject {
  Q_OBJECT

 private slots:
  void detailPanelShouldReflectSelection();
  void exportReportShouldWriteTaskSummaryToFile();
  void filterShouldSupportQueuedState();
  void filterShouldSupportCanceledState();
  void actionButtonsShouldFollowTaskState();
  void keywordFilterShouldMatchSummaryAndDetail();
};

void TaskCenterWidgetTest::detailPanelShouldReflectSelection() {
  pdftools_gui::models::TaskItemModel model;
  pdftools_gui::widgets::TaskCenterWidget widget;
  widget.SetSourceModel(&model);
  widget.show();

  pdftools_gui::services::TaskInfo info;
  info.id = 1;
  info.operation = pdftools_gui::services::OperationKind::kExtractText;
  info.state = pdftools_gui::services::TaskState::kFailed;
  info.summary = QStringLiteral("summary-1");
  info.detail = QStringLiteral("detail-1");
  info.output_path = QStringLiteral("/tmp/out-1.txt");
  model.UpsertTask(info);

  auto* table = widget.findChild<QTableView*>();
  QVERIFY(table != nullptr);
  table->selectRow(0);
  table->setCurrentIndex(table->model()->index(0, 0));

  QCoreApplication::processEvents();

  auto* detail = widget.findChild<QPlainTextEdit*>();
  QVERIFY(detail != nullptr);
  QVERIFY(detail->toPlainText().contains(QStringLiteral("summary-1")));
  QVERIFY(detail->toPlainText().contains(QStringLiteral("detail-1")));
  QVERIFY(detail->toPlainText().contains(QStringLiteral("/tmp/out-1.txt")));
}

void TaskCenterWidgetTest::exportReportShouldWriteTaskSummaryToFile() {
  QTemporaryDir temp_dir;
  QVERIFY2(temp_dir.isValid(), "failed to create temp directory");

  pdftools_gui::models::TaskItemModel model;
  pdftools_gui::widgets::TaskCenterWidget widget;
  widget.SetReportDirectory(temp_dir.path());
  widget.SetSourceModel(&model);
  widget.show();

  pdftools_gui::services::TaskInfo info;
  info.id = 9;
  info.operation = pdftools_gui::services::OperationKind::kPdfToDocx;
  info.state = pdftools_gui::services::TaskState::kFailed;
  info.summary = QStringLiteral("convert failed");
  info.detail = QStringLiteral("image relation missing");
  info.output_path = QStringLiteral("/tmp/final.docx");
  model.UpsertTask(info);

  auto* table = widget.findChild<QTableView*>();
  QVERIFY(table != nullptr);
  table->selectRow(0);
  table->setCurrentIndex(table->model()->index(0, 0));

  auto* export_button = widget.findChild<QPushButton*>(QStringLiteral("export_report_button"));
  QVERIFY(export_button != nullptr);
  QTest::mouseClick(export_button, Qt::LeftButton);
  QCoreApplication::processEvents();

  const QString report_path = widget.LastExportedReportPath();
  QVERIFY(!report_path.isEmpty());
  QVERIFY(QFile::exists(report_path));

  QFile report_file(report_path);
  QVERIFY(report_file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString content = QString::fromUtf8(report_file.readAll());
  report_file.close();

  QVERIFY(content.contains(QStringLiteral("TaskId: 9")));
  QVERIFY(content.contains(QStringLiteral("convert failed")));
  QVERIFY(content.contains(QStringLiteral("image relation missing")));
  QVERIFY(content.contains(QStringLiteral("/tmp/final.docx")));
}

void TaskCenterWidgetTest::filterShouldSupportQueuedState() {
  pdftools_gui::models::TaskItemModel model;
  pdftools_gui::widgets::TaskCenterWidget widget;
  widget.SetSourceModel(&model);
  widget.show();

  pdftools_gui::services::TaskInfo queued;
  queued.id = 11;
  queued.state = pdftools_gui::services::TaskState::kQueued;
  queued.summary = QStringLiteral("queued");
  model.UpsertTask(queued);

  pdftools_gui::services::TaskInfo failed;
  failed.id = 12;
  failed.state = pdftools_gui::services::TaskState::kFailed;
  failed.summary = QStringLiteral("failed");
  model.UpsertTask(failed);

  auto* filter_combo = widget.findChild<QComboBox*>(QStringLiteral("task_filter_combo"));
  auto* table = widget.findChild<QTableView*>();
  QVERIFY(filter_combo != nullptr);
  QVERIFY(table != nullptr);

  filter_combo->setCurrentIndex(1);  // Queued
  QCoreApplication::processEvents();

  QCOMPARE(table->model()->rowCount(), 1);
  QCOMPARE(table->model()->data(table->model()->index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 11LL);
}

void TaskCenterWidgetTest::filterShouldSupportCanceledState() {
  pdftools_gui::models::TaskItemModel model;
  pdftools_gui::widgets::TaskCenterWidget widget;
  widget.SetSourceModel(&model);
  widget.show();

  pdftools_gui::services::TaskInfo canceled;
  canceled.id = 31;
  canceled.state = pdftools_gui::services::TaskState::kCanceled;
  canceled.summary = QStringLiteral("canceled");
  model.UpsertTask(canceled);

  pdftools_gui::services::TaskInfo running;
  running.id = 32;
  running.state = pdftools_gui::services::TaskState::kRunning;
  running.summary = QStringLiteral("running");
  model.UpsertTask(running);

  auto* filter_combo = widget.findChild<QComboBox*>(QStringLiteral("task_filter_combo"));
  auto* table = widget.findChild<QTableView*>();
  QVERIFY(filter_combo != nullptr);
  QVERIFY(table != nullptr);

  filter_combo->setCurrentIndex(5);  // Canceled
  QCoreApplication::processEvents();

  QCOMPARE(table->model()->rowCount(), 1);
  QCOMPARE(table->model()->data(table->model()->index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 31LL);
}

void TaskCenterWidgetTest::actionButtonsShouldFollowTaskState() {
  pdftools_gui::models::TaskItemModel model;
  pdftools_gui::widgets::TaskCenterWidget widget;
  widget.SetSourceModel(&model);
  widget.show();

  auto* table = widget.findChild<QTableView*>();
  auto* cancel_button = widget.findChild<QPushButton*>(QStringLiteral("cancel_task_button"));
  auto* export_button = widget.findChild<QPushButton*>(QStringLiteral("export_report_button"));
  auto* open_button = widget.findChild<QPushButton*>(QStringLiteral("open_output_button"));
  QVERIFY(table != nullptr);
  QVERIFY(cancel_button != nullptr);
  QVERIFY(export_button != nullptr);
  QVERIFY(open_button != nullptr);

  QVERIFY(!cancel_button->isEnabled());
  QVERIFY(!export_button->isEnabled());
  QVERIFY(!open_button->isEnabled());

  pdftools_gui::services::TaskInfo queued;
  queued.id = 21;
  queued.state = pdftools_gui::services::TaskState::kQueued;
  queued.summary = QStringLiteral("queued");
  model.UpsertTask(queued);

  pdftools_gui::services::TaskInfo failed;
  failed.id = 22;
  failed.state = pdftools_gui::services::TaskState::kFailed;
  failed.summary = QStringLiteral("failed");
  model.UpsertTask(failed);

  table->selectRow(0);  // failed
  table->setCurrentIndex(table->model()->index(0, 0));
  QCoreApplication::processEvents();
  QVERIFY(!cancel_button->isEnabled());
  QVERIFY(export_button->isEnabled());
  QVERIFY(open_button->isEnabled());

  table->selectRow(1);  // queued
  table->setCurrentIndex(table->model()->index(1, 0));
  QCoreApplication::processEvents();
  QVERIFY(cancel_button->isEnabled());
  QVERIFY(export_button->isEnabled());
  QVERIFY(open_button->isEnabled());
}

void TaskCenterWidgetTest::keywordFilterShouldMatchSummaryAndDetail() {
  pdftools_gui::models::TaskItemModel model;
  pdftools_gui::widgets::TaskCenterWidget widget;
  widget.SetSourceModel(&model);
  widget.show();

  pdftools_gui::services::TaskInfo t1;
  t1.id = 41;
  t1.state = pdftools_gui::services::TaskState::kFailed;
  t1.summary = QStringLiteral("merge failed");
  t1.detail = QStringLiteral("missing file");
  model.UpsertTask(t1);

  pdftools_gui::services::TaskInfo t2;
  t2.id = 42;
  t2.state = pdftools_gui::services::TaskState::kSucceeded;
  t2.summary = QStringLiteral("extract done");
  t2.detail = QStringLiteral("entries=120");
  model.UpsertTask(t2);

  auto* table = widget.findChild<QTableView*>();
  auto* search_edit = widget.findChild<QLineEdit*>(QStringLiteral("task_search_edit"));
  QVERIFY(table != nullptr);
  QVERIFY(search_edit != nullptr);

  search_edit->setText(QStringLiteral("entries=120"));
  QCoreApplication::processEvents();
  QCOMPARE(table->model()->rowCount(), 1);
  QCOMPARE(table->model()->data(table->model()->index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 42LL);

  search_edit->setText(QStringLiteral("merge"));
  QCoreApplication::processEvents();
  QCOMPARE(table->model()->rowCount(), 1);
  QCOMPARE(table->model()->data(table->model()->index(0, pdftools_gui::models::TaskItemModel::kId)).toLongLong(), 41LL);

  search_edit->clear();
  QCoreApplication::processEvents();
  QCOMPARE(table->model()->rowCount(), 2);
}

}  // namespace

QTEST_MAIN(TaskCenterWidgetTest)
#include "task_center_widget_test.moc"
