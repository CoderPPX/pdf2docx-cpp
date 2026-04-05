#include <QApplication>
#include <QTimer>

#include "qt_pdftools/app/main_window.hpp"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  qt_pdftools::app::MainWindow window;
  window.show();

  bool ok = false;
  const int auto_close_ms = qgetenv("QT_PDFTOOLS_AUTOCLOSE_MS").toInt(&ok);
  if (ok && auto_close_ms >= 0) {
    QTimer::singleShot(auto_close_ms, &window, &QWidget::close);
    QTimer::singleShot(auto_close_ms + 50, &app, &QCoreApplication::quit);
  }

  return app.exec();
}
