#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include "pdftools_gui/services/task_manager.hpp"

namespace pdftools_gui::services {

class SettingsService {
 public:
  SettingsService();

  QVariant ReadValue(const QString& key, const QVariant& default_value = {}) const;
  void WriteValue(const QString& key, const QVariant& value);

  QString LastDirectory(const QString& context_key) const;
  void SetLastDirectory(const QString& context_key, const QString& path);

  QString RecentValue(const QString& key) const;
  QStringList RecentValues(const QString& key) const;
  void PushRecentValue(const QString& key, const QString& value, int max_items = 12);
  void ClearByPrefix(const QString& prefix);
  QStringList AllKeys() const;
  void RemoveKey(const QString& key);
  void WriteTaskHistory(const QVector<pdftools_gui::services::TaskInfo>& tasks, int max_items = 200);
  QVector<pdftools_gui::services::TaskInfo> ReadTaskHistory(int max_items = 200) const;
  void ClearTaskHistory();

 private:
  mutable QSettings settings_;
};

}  // namespace pdftools_gui::services
