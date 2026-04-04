#pragma once

#include <QString>
#include <QWidget>

#include "pdftools_gui/services/task_manager.hpp"

class QComboBox;
class QItemSelection;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;

namespace pdftools_gui::models {
class TaskItemModel;
}

namespace pdftools_gui::widgets {

class TaskCenterWidget : public QWidget {
  Q_OBJECT

 public:
  explicit TaskCenterWidget(QWidget* parent = nullptr);

  void SetTaskManager(pdftools_gui::services::TaskManager* task_manager);
  void SetSourceModel(pdftools_gui::models::TaskItemModel* model);
  void SetReportDirectory(const QString& report_directory);
  QString LastExportedReportPath() const;

 private:
  void ApplyFilter(int index);
  void ApplyKeywordFilter(const QString& keyword);
  void CancelSelectedTask();
  void ExportSelectedReport();
  void OpenSelectedOutput();
  void UpdateDetailPanel();
  void UpdateActionButtons();
  QString ResolveReportDirectory() const;

  QComboBox* filter_combo_ = nullptr;
  QLineEdit* search_edit_ = nullptr;
  QPushButton* cancel_task_button_ = nullptr;
  QPushButton* export_report_button_ = nullptr;
  QPushButton* open_output_button_ = nullptr;
  QTableView* table_view_ = nullptr;
  QPlainTextEdit* detail_view_ = nullptr;
  QSortFilterProxyModel* proxy_model_ = nullptr;
  pdftools_gui::services::TaskManager* task_manager_ = nullptr;
  QString report_directory_;
  QString last_exported_report_path_;
};

}  // namespace pdftools_gui::widgets
