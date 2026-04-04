#include "pdftools_gui/services/validation_service.hpp"

#include <QDir>
#include <QFileInfo>

namespace pdftools_gui::services {

QString ValidationService::ValidateExistingFile(const QString& path, const QString& field_label) {
  if (path.trimmed().isEmpty()) {
    return QStringLiteral("%1不能为空").arg(field_label);
  }

  QFileInfo info(path);
  if (!info.exists() || !info.isFile()) {
    return QStringLiteral("%1不存在或不是文件: %2").arg(field_label, path);
  }

  return {};
}

QString ValidationService::ValidateOutputFile(const QString& path, bool overwrite, const QString& field_label) {
  if (path.trimmed().isEmpty()) {
    return QStringLiteral("%1不能为空").arg(field_label);
  }

  QFileInfo info(path);
  const QDir parent = info.dir();
  if (!parent.exists()) {
    if (!QDir().mkpath(parent.absolutePath())) {
      return QStringLiteral("无法创建输出目录: %1").arg(parent.absolutePath());
    }
  }

  if (info.exists() && !overwrite) {
    return QStringLiteral("%1已存在，请启用覆盖或更换路径: %2").arg(field_label, path);
  }

  return {};
}

QString ValidationService::ValidateDirectory(const QString& path, const QString& field_label) {
  if (path.trimmed().isEmpty()) {
    return QStringLiteral("%1不能为空").arg(field_label);
  }

  QDir dir(path);
  if (!dir.exists()) {
    if (!QDir().mkpath(path)) {
      return QStringLiteral("无法创建目录: %1").arg(path);
    }
  }

  return {};
}

QString ValidationService::ValidateNonEmptyList(const QStringList& values, const QString& field_label) {
  if (values.isEmpty()) {
    return QStringLiteral("%1至少需要一项").arg(field_label);
  }

  for (const QString& value : values) {
    if (value.trimmed().isEmpty()) {
      return QStringLiteral("%1包含空项").arg(field_label);
    }
  }
  return {};
}

QString ValidationService::ValidatePageNumber(int value, const QString& field_label) {
  if (value <= 0) {
    return QStringLiteral("%1必须是正整数").arg(field_label);
  }
  return {};
}

}  // namespace pdftools_gui::services
