#include "pdftools_gui/services/settings_service.hpp"

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr const char* kTaskHistoryJsonKey = "history/tasks_json_v1";

}  // namespace

namespace pdftools_gui::services {

SettingsService::SettingsService() : settings_(QCoreApplication::organizationName(), QCoreApplication::applicationName()) {}

QVariant SettingsService::ReadValue(const QString& key, const QVariant& default_value) const {
  return settings_.value(key, default_value);
}

void SettingsService::WriteValue(const QString& key, const QVariant& value) {
  settings_.setValue(key, value);
}

QString SettingsService::LastDirectory(const QString& context_key) const {
  const QString key = QStringLiteral("paths/%1").arg(context_key);
  const QString path = settings_.value(key, QString()).toString();
  if (path.isEmpty()) {
    return QDir::homePath();
  }
  return path;
}

void SettingsService::SetLastDirectory(const QString& context_key, const QString& path) {
  if (path.isEmpty()) {
    return;
  }

  QFileInfo info(path);
  QString directory = path;
  if (info.exists() && info.isFile()) {
    directory = info.absolutePath();
  }

  settings_.setValue(QStringLiteral("paths/%1").arg(context_key), directory);
}

QString SettingsService::RecentValue(const QString& key) const {
  const QStringList values = RecentValues(key);
  if (values.isEmpty()) {
    return {};
  }
  return values.front();
}

QStringList SettingsService::RecentValues(const QString& key) const {
  return settings_.value(QStringLiteral("recent/%1").arg(key), QStringList()).toStringList();
}

void SettingsService::PushRecentValue(const QString& key, const QString& value, int max_items) {
  if (value.trimmed().isEmpty()) {
    return;
  }

  QStringList values = RecentValues(key);
  values.removeAll(value);
  values.prepend(value);
  while (values.size() > max_items) {
    values.removeLast();
  }

  settings_.setValue(QStringLiteral("recent/%1").arg(key), values);
}

void SettingsService::ClearByPrefix(const QString& prefix) {
  const QString normalized_prefix = prefix.endsWith('/') ? prefix : prefix + '/';
  for (const QString& key : settings_.allKeys()) {
    if (key.startsWith(normalized_prefix)) {
      settings_.remove(key);
    }
  }
}

QStringList SettingsService::AllKeys() const {
  return settings_.allKeys();
}

void SettingsService::RemoveKey(const QString& key) {
  settings_.remove(key);
}

void SettingsService::WriteTaskHistory(const QVector<pdftools_gui::services::TaskInfo>& tasks, int max_items) {
  const int cap = std::max(0, max_items);
  QJsonArray array;
  const int count = std::min(cap, static_cast<int>(tasks.size()));
  for (int i = 0; i < count; ++i) {
    const auto& task = tasks.at(i);
    QJsonObject object;
    object.insert(QStringLiteral("id"), static_cast<qint64>(task.id));
    object.insert(QStringLiteral("operation"), static_cast<int>(task.operation));
    object.insert(QStringLiteral("display_name"), task.display_name);
    object.insert(QStringLiteral("state"), static_cast<int>(task.state));
    object.insert(QStringLiteral("progress"), task.progress);
    object.insert(QStringLiteral("submitted_at"), task.submitted_at.toString(Qt::ISODate));
    object.insert(QStringLiteral("finished_at"), task.finished_at.toString(Qt::ISODate));
    object.insert(QStringLiteral("summary"), task.summary);
    object.insert(QStringLiteral("detail"), task.detail);
    object.insert(QStringLiteral("output_path"), task.output_path);
    array.push_back(object);
  }
  settings_.setValue(QString::fromUtf8(kTaskHistoryJsonKey), QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<pdftools_gui::services::TaskInfo> SettingsService::ReadTaskHistory(int max_items) const {
  const int cap = std::max(0, max_items);
  QVector<pdftools_gui::services::TaskInfo> tasks;
  if (cap == 0) {
    return tasks;
  }

  const QByteArray raw = settings_.value(QString::fromUtf8(kTaskHistoryJsonKey), QByteArray()).toByteArray();
  if (raw.isEmpty()) {
    return tasks;
  }

  QJsonParseError parse_error{};
  const QJsonDocument doc = QJsonDocument::fromJson(raw, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isArray()) {
    return tasks;
  }

  const QJsonArray array = doc.array();
  tasks.reserve(std::min(cap, static_cast<int>(array.size())));
  for (int i = 0; i < array.size() && tasks.size() < cap; ++i) {
    if (!array.at(i).isObject()) {
      continue;
    }
    const QJsonObject object = array.at(i).toObject();
    TaskInfo task;
    task.id = object.value(QStringLiteral("id")).toVariant().toLongLong();
    task.operation = static_cast<OperationKind>(object.value(QStringLiteral("operation")).toInt());
    task.display_name = object.value(QStringLiteral("display_name")).toString();
    task.state = static_cast<TaskState>(object.value(QStringLiteral("state")).toInt());
    task.progress = object.value(QStringLiteral("progress")).toInt();
    task.submitted_at = QDateTime::fromString(object.value(QStringLiteral("submitted_at")).toString(), Qt::ISODate);
    task.finished_at = QDateTime::fromString(object.value(QStringLiteral("finished_at")).toString(), Qt::ISODate);
    task.summary = object.value(QStringLiteral("summary")).toString();
    task.detail = object.value(QStringLiteral("detail")).toString();
    task.output_path = object.value(QStringLiteral("output_path")).toString();
    tasks.push_back(task);
  }
  return tasks;
}

void SettingsService::ClearTaskHistory() {
  settings_.remove(QString::fromUtf8(kTaskHistoryJsonKey));
}

}  // namespace pdftools_gui::services
