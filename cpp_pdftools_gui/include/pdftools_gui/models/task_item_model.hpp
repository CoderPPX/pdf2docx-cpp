#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "pdftools_gui/services/task_manager.hpp"

namespace pdftools_gui::models {

class TaskItemModel : public QAbstractTableModel {
  Q_OBJECT

 public:
  enum Column {
    kId = 0,
    kOperation,
    kStatus,
    kProgress,
    kSubmittedAt,
    kFinishedAt,
    kSummary,
    kColumnCount,
  };

  enum Role {
    kTaskIdRole = Qt::UserRole + 1,
    kTaskStateRole,
    kOutputPathRole,
    kDetailRole,
  };

  explicit TaskItemModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

  void UpsertTask(const pdftools_gui::services::TaskInfo& info);
  QVector<pdftools_gui::services::TaskInfo> SnapshotTasks() const;
  void ReplaceAllTasks(const QVector<pdftools_gui::services::TaskInfo>& tasks);
  void RemoveFinalTasks();

 private:
  int FindRowByTaskId(qint64 task_id) const;

  QVector<pdftools_gui::services::TaskInfo> tasks_;
};

}  // namespace pdftools_gui::models
