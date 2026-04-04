#pragma once

#include <QString>
#include <QStringList>

namespace pdftools_gui::services {

class ValidationService {
 public:
  static QString ValidateExistingFile(const QString& path, const QString& field_label);
  static QString ValidateOutputFile(const QString& path, bool overwrite, const QString& field_label);
  static QString ValidateDirectory(const QString& path, const QString& field_label);
  static QString ValidateNonEmptyList(const QStringList& values, const QString& field_label);
  static QString ValidatePageNumber(int value, const QString& field_label);
};

}  // namespace pdftools_gui::services
