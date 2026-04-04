#include <QApplication>
#include <QCoreApplication>

#include "pdftools_gui/app/main_window.hpp"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("pdftools"));
  QCoreApplication::setApplicationName(QStringLiteral("pdftools-gui"));

  pdftools_gui::app::MainWindow window;
  window.show();
  return app.exec();
}
