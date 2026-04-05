#include "qt_pdftools/app/main_window.hpp"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

#include <QtConcurrent>

#include "qt_pdftools/backend/task_backend.hpp"
#include "qt_pdftools/core/app_settings.hpp"
#include "qt_pdftools/ui/pdf_tab_view.hpp"
#include "qt_pdftools/ui/settings_dialog.hpp"

namespace qt_pdftools::app {
namespace {

QString NowTime() {
  return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

QString Basename(const QString& path) {
  return QFileInfo(path).fileName();
}

void FillComboWithPaths(QComboBox* combo,
                        const QStringList& paths,
                        const QString& current,
                        const QString& active_path) {
  if (combo == nullptr) {
    return;
  }

  combo->blockSignals(true);
  combo->clear();
  combo->addItem(QStringLiteral("-- 请选择已打开 Tab --"), QString());
  for (const QString& path : paths) {
    combo->addItem(Basename(path), path);
  }

  int index = combo->findData(current);
  if (index < 0 && !active_path.isEmpty()) {
    index = combo->findData(active_path);
  }
  combo->setCurrentIndex(std::max(0, index));
  combo->blockSignals(false);
}

struct LogStyle {
  QString level;
  QString badge_color;
  QString dark_text_color;
  QString light_text_color;
};

LogStyle ResolveLogStyle(const QString& message) {
  const QString lower = message.toLower();
  if (lower.contains(QStringLiteral("stderr")) || lower.contains(QStringLiteral("失败")) ||
      lower.contains(QStringLiteral("error")) || lower.contains(QStringLiteral("异常"))) {
    return {QStringLiteral("ERROR"), QStringLiteral("#e35d6a"), QStringLiteral("#ffd6db"), QStringLiteral("#b4233e")};
  }
  if (lower.contains(QStringLiteral("警告")) || lower.contains(QStringLiteral("warning")) ||
      lower.contains(QStringLiteral("未就绪")) || lower.contains(QStringLiteral("取消"))) {
    return {QStringLiteral("WARN"), QStringLiteral("#d39c32"), QStringLiteral("#ffe7b5"), QStringLiteral("#8a5d0a")};
  }
  if (lower.contains(QStringLiteral("成功")) || lower.contains(QStringLiteral("完成"))) {
    return {QStringLiteral("OK"), QStringLiteral("#4aa96c"), QStringLiteral("#d7ffe5"), QStringLiteral("#166534")};
  }
  if (lower.contains(QStringLiteral("开始任务")) || lower.contains(QStringLiteral("运行")) ||
      lower.contains(QStringLiteral("加载"))) {
    return {QStringLiteral("TASK"), QStringLiteral("#4c87ff"), QStringLiteral("#d9e6ff"), QStringLiteral("#1d4ed8")};
  }
  return {QStringLiteral("INFO"), QStringLiteral("#5f6b7a"), QStringLiteral("#e6ecf4"), QStringLiteral("#334155")};
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  settings_ = std::make_unique<QSettings>(QStringLiteral("pdftools"), QStringLiteral("qt_pdftools"));
  app_settings_ = qt_pdftools::core::LoadAppSettings(settings_.get());

  if (backend_registry_.Get(active_backend_id_) == nullptr) {
    const QStringList ids = backend_registry_.BackendIds();
    if (!ids.isEmpty()) {
      active_backend_id_ = ids.first();
    }
  }

  BuildUi();
  BindSignals();
  ApplyTheme(app_settings_.theme_mode);
  const QString preload_path = QString::fromLocal8Bit(qgetenv("QT_PDFTOOLS_OPEN_ON_START")).trimmed();
  if (!preload_path.isEmpty()) {
    OpenPdfTabs(QStringList{preload_path}, true);
  }
  RefreshBackendStatus(true);
  AppendLog(QStringLiteral("UI 已初始化。"));
}

MainWindow::~MainWindow() {
  if (task_watcher_ != nullptr) {
    task_watcher_->disconnect(this);
    task_watcher_->waitForFinished();
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (preview_tabs_ != nullptr) {
    while (preview_tabs_->count() > 0) {
      if (!RequestCloseTab(preview_tabs_->count() - 1)) {
        event->ignore();
        return;
      }
    }
    preview_tabs_->disconnect(this);
    preview_tabs_->blockSignals(true);
  }

  if (task_watcher_ != nullptr) {
    AppendLog(QStringLiteral("窗口关闭：等待正在运行的任务完成..."));
    task_watcher_->disconnect(this);
    task_watcher_->waitForFinished();
    running_task_ = false;
  }

  QMainWindow::closeEvent(event);
}

void MainWindow::BuildUi() {
  setWindowTitle(QStringLiteral("Qt PDFTools"));
  resize(1500, 920);

  BuildMenu();

  auto* splitter = new QSplitter(this);
  splitter->setOrientation(Qt::Horizontal);
  splitter->addWidget(BuildSidebar());
  splitter->addWidget(BuildPreviewArea());
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({420, 1080});
  setCentralWidget(splitter);

  statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::BuildMenu() {
  auto* file_menu = menuBar()->addMenu(QStringLiteral("File"));

  auto* open_pdf_action = file_menu->addAction(QStringLiteral("Open PDF to New Tab..."));
  auto* save_action = file_menu->addAction(QStringLiteral("Save"));
  auto* save_as_action = file_menu->addAction(QStringLiteral("Save As..."));
  file_menu->addSeparator();
  auto* close_current_action = file_menu->addAction(QStringLiteral("Close Current Tab"));
  auto* close_all_action = file_menu->addAction(QStringLiteral("Close All Tabs"));
  file_menu->addSeparator();
  auto* settings_action = file_menu->addAction(QStringLiteral("Settings..."));
  file_menu->addSeparator();
  auto* exit_action = file_menu->addAction(QStringLiteral("Exit"));

  open_pdf_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Open));
  save_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Save));
  save_as_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::SaveAs));
  close_current_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Close));
  close_all_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
  settings_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Preferences));
  exit_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Quit));

  connect(open_pdf_action, &QAction::triggered, this, [this]() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择 PDF 文件"),
        DialogInitialPath(),
        QStringLiteral("PDF Files (*.pdf)"));
    if (!files.isEmpty()) {
      RememberDialogPath(files.first());
    }
    OpenPdfTabs(files, true);
  });

  connect(save_action, &QAction::triggered, this, [this]() {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可保存的 Tab。"));
      return;
    }
    SaveTabByIndex(preview_tabs_->currentIndex(), false);
  });

  connect(save_as_action, &QAction::triggered, this, [this]() {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可另存为的 Tab。"));
      return;
    }
    SaveTabByIndex(preview_tabs_->currentIndex(), true);
  });

  connect(close_current_action, &QAction::triggered, this, &MainWindow::CloseCurrentPdfTab);
  connect(close_all_action, &QAction::triggered, this, &MainWindow::CloseAllPdfTabs);
  connect(settings_action, &QAction::triggered, this, &MainWindow::OpenSettingsDialog);
  connect(exit_action, &QAction::triggered, this, &QWidget::close);
}

QWidget* MainWindow::BuildSidebar() {
  auto* sidebar = new QWidget(this);
  auto* root = new QVBoxLayout(sidebar);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  tool_tabs_ = new QTabWidget(sidebar);
  tool_tabs_->addTab(BuildMergePage(), QStringLiteral("合并"));
  tool_tabs_->addTab(BuildDeletePage(), QStringLiteral("删除页"));
  tool_tabs_->addTab(BuildInsertPage(), QStringLiteral("插入页"));
  tool_tabs_->addTab(BuildReplacePage(), QStringLiteral("替换页"));
  tool_tabs_->addTab(BuildDocxPage(), QStringLiteral("PDF2DOCX"));
  root->addWidget(tool_tabs_, 1);

  auto* log_group = new QGroupBox(QStringLiteral("日志"), sidebar);
  auto* log_layout = new QVBoxLayout(log_group);
  task_log_ = new QTextEdit(log_group);
  task_log_->setReadOnly(true);
  task_log_->document()->setMaximumBlockCount(2000);
  task_log_->setMinimumHeight(220);
  task_log_->setPlaceholderText(QStringLiteral("日志输出将在这里显示。"));
  log_layout->addWidget(task_log_, 1);
  root->addWidget(log_group, 1);

  return sidebar;
}

QWidget* MainWindow::BuildMergePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  merge_tab_select_ = new QListWidget(page);
  merge_tab_select_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  merge_tab_select_->setMinimumHeight(92);

  auto* use_tabs_button = new QPushButton(QStringLiteral("使用所选 Tab 填充输入"), page);
  connect(use_tabs_button, &QPushButton::clicked, this, [this]() {
    QStringList selected_paths;
    const QList<QListWidgetItem*> items = merge_tab_select_->selectedItems();
    for (QListWidgetItem* item : items) {
      selected_paths << item->data(Qt::UserRole).toString();
    }
    if (selected_paths.isEmpty()) {
      AppendLog(QStringLiteral("未选择任何 Tab。"));
      return;
    }
    merge_inputs_edit_->setPlainText(selected_paths.join(QStringLiteral("\n")));
    AppendLog(QStringLiteral("已用 %1 个 Tab 填充合并输入。").arg(selected_paths.size()));
  });

  merge_inputs_edit_ = new QTextEdit(page);
  merge_inputs_edit_->setPlaceholderText(QStringLiteral("每行一个 PDF 路径"));
  merge_inputs_edit_->setMinimumHeight(100);

  auto* output_row = new QHBoxLayout();
  merge_output_edit_ = new QLineEdit(page);
  auto* output_browse = new QPushButton(QStringLiteral("选择输出"), page);
  output_row->addWidget(merge_output_edit_, 1);
  output_row->addWidget(output_browse);
  connect(output_browse, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存合并 PDF"),
        DialogInitialPath(merge_output_edit_->text()),
        QStringLiteral("PDF Files (*.pdf)"));
    if (!target.isEmpty()) {
      merge_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  merge_run_button_ = new QPushButton(QStringLiteral("执行合并"), page);
  merge_preview_button_ = new QPushButton(QStringLiteral("预览结果"), page);
  auto* action_row = new QHBoxLayout();
  action_row->addWidget(merge_preview_button_);
  action_row->addWidget(merge_run_button_);

  layout->addWidget(new QLabel(QStringLiteral("从已打开 Tab 选择（可多选）"), page));
  layout->addWidget(merge_tab_select_);
  layout->addWidget(use_tabs_button);
  layout->addWidget(new QLabel(QStringLiteral("输入文件（每行一个）"), page));
  layout->addWidget(merge_inputs_edit_);
  layout->addWidget(new QLabel(QStringLiteral("输出文件"), page));
  layout->addLayout(output_row);
  layout->addLayout(action_row);
  layout->addStretch(1);

  return page;
}

QWidget* MainWindow::BuildDeletePage() {
  auto* page = new QWidget(this);
  auto* layout = new QFormLayout(page);

  delete_input_tab_combo_ = new QComboBox(page);
  delete_input_edit_ = new QLineEdit(page);
  delete_output_edit_ = new QLineEdit(page);
  delete_page_spin_ = new QSpinBox(page);
  delete_page_spin_->setRange(1, 999999);
  delete_page_spin_->setValue(1);

  auto* pick_input = new QPushButton(QStringLiteral("选择输入"), page);
  auto* pick_output = new QPushButton(QStringLiteral("选择输出"), page);
  delete_run_button_ = new QPushButton(QStringLiteral("执行"), page);
  delete_preview_button_ = new QPushButton(QStringLiteral("预览"), page);

  connect(pick_input, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择输入 PDF"),
                                                      DialogInitialPath(delete_input_edit_->text()),
                                                      QStringLiteral("PDF Files (*.pdf)"));
    if (!file.isEmpty()) {
      RememberDialogPath(file);
      delete_input_edit_->setText(file);
      OpenPdfTab(file, true);
      if (delete_output_edit_->text().trimmed().isEmpty()) {
        delete_output_edit_->setText(SuggestOutput(file, QStringLiteral("_delete"), QStringLiteral(".pdf")));
      }
    }
  });

  connect(pick_output, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("保存输出 PDF"),
                                                        DialogInitialPath(delete_output_edit_->text()),
                                                        QStringLiteral("PDF Files (*.pdf)"));
    if (!target.isEmpty()) {
      delete_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* input_row = new QHBoxLayout();
  input_row->addWidget(delete_input_edit_, 1);
  input_row->addWidget(pick_input);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(delete_output_edit_, 1);
  output_row->addWidget(pick_output);

  layout->addRow(QStringLiteral("从 Tab 选择输入"), delete_input_tab_combo_);
  layout->addRow(QStringLiteral("输入 PDF"), input_row);
  layout->addRow(QStringLiteral("输出 PDF"), output_row);
  layout->addRow(QStringLiteral("删除页（1-based）"), delete_page_spin_);
  auto* action_row = new QHBoxLayout();
  action_row->addWidget(delete_preview_button_);
  action_row->addWidget(delete_run_button_);
  layout->addRow(action_row);

  return page;
}

QWidget* MainWindow::BuildInsertPage() {
  auto* page = new QWidget(this);
  auto* layout = new QFormLayout(page);

  insert_target_tab_combo_ = new QComboBox(page);
  insert_target_edit_ = new QLineEdit(page);
  insert_source_tab_combo_ = new QComboBox(page);
  insert_source_edit_ = new QLineEdit(page);
  insert_output_edit_ = new QLineEdit(page);

  insert_at_spin_ = new QSpinBox(page);
  insert_at_spin_->setRange(1, 999999);
  insert_at_spin_->setValue(1);
  insert_source_page_spin_ = new QSpinBox(page);
  insert_source_page_spin_->setRange(1, 999999);
  insert_source_page_spin_->setValue(1);

  auto* pick_target = new QPushButton(QStringLiteral("选择目标"), page);
  auto* pick_source = new QPushButton(QStringLiteral("选择来源"), page);
  auto* pick_output = new QPushButton(QStringLiteral("选择输出"), page);
  insert_run_button_ = new QPushButton(QStringLiteral("执行"), page);
  insert_preview_button_ = new QPushButton(QStringLiteral("预览"), page);

  connect(pick_target, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择目标 PDF"),
                                                      DialogInitialPath(insert_target_edit_->text()),
                                                      QStringLiteral("PDF Files (*.pdf)"));
    if (!file.isEmpty()) {
      RememberDialogPath(file);
      insert_target_edit_->setText(file);
      OpenPdfTab(file, true);
      if (insert_output_edit_->text().trimmed().isEmpty()) {
        insert_output_edit_->setText(SuggestOutput(file, QStringLiteral("_insert"), QStringLiteral(".pdf")));
      }
    }
  });

  connect(pick_source, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择来源 PDF"),
                                                      DialogInitialPath(insert_source_edit_->text()),
                                                      QStringLiteral("PDF Files (*.pdf)"));
    if (!file.isEmpty()) {
      RememberDialogPath(file);
      insert_source_edit_->setText(file);
      OpenPdfTab(file, true);
    }
  });

  connect(pick_output, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("保存输出 PDF"),
                                                        DialogInitialPath(insert_output_edit_->text()),
                                                        QStringLiteral("PDF Files (*.pdf)"));
    if (!target.isEmpty()) {
      insert_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* target_row = new QHBoxLayout();
  target_row->addWidget(insert_target_edit_, 1);
  target_row->addWidget(pick_target);

  auto* source_row = new QHBoxLayout();
  source_row->addWidget(insert_source_edit_, 1);
  source_row->addWidget(pick_source);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(insert_output_edit_, 1);
  output_row->addWidget(pick_output);

  layout->addRow(QStringLiteral("从 Tab 选择目标"), insert_target_tab_combo_);
  layout->addRow(QStringLiteral("目标 PDF"), target_row);
  layout->addRow(QStringLiteral("从 Tab 选择来源"), insert_source_tab_combo_);
  layout->addRow(QStringLiteral("来源 PDF"), source_row);
  layout->addRow(QStringLiteral("输出 PDF"), output_row);
  layout->addRow(QStringLiteral("插入位置 at"), insert_at_spin_);
  layout->addRow(QStringLiteral("来源页 source-page"), insert_source_page_spin_);
  auto* action_row = new QHBoxLayout();
  action_row->addWidget(insert_preview_button_);
  action_row->addWidget(insert_run_button_);
  layout->addRow(action_row);

  return page;
}

QWidget* MainWindow::BuildReplacePage() {
  auto* page = new QWidget(this);
  auto* layout = new QFormLayout(page);

  replace_target_tab_combo_ = new QComboBox(page);
  replace_target_edit_ = new QLineEdit(page);
  replace_source_tab_combo_ = new QComboBox(page);
  replace_source_edit_ = new QLineEdit(page);
  replace_output_edit_ = new QLineEdit(page);

  replace_page_spin_ = new QSpinBox(page);
  replace_page_spin_->setRange(1, 999999);
  replace_page_spin_->setValue(1);
  replace_source_page_spin_ = new QSpinBox(page);
  replace_source_page_spin_->setRange(1, 999999);
  replace_source_page_spin_->setValue(1);

  auto* pick_target = new QPushButton(QStringLiteral("选择目标"), page);
  auto* pick_source = new QPushButton(QStringLiteral("选择来源"), page);
  auto* pick_output = new QPushButton(QStringLiteral("选择输出"), page);
  replace_run_button_ = new QPushButton(QStringLiteral("执行"), page);
  replace_preview_button_ = new QPushButton(QStringLiteral("预览"), page);

  connect(pick_target, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择目标 PDF"),
                                                      DialogInitialPath(replace_target_edit_->text()),
                                                      QStringLiteral("PDF Files (*.pdf)"));
    if (!file.isEmpty()) {
      RememberDialogPath(file);
      replace_target_edit_->setText(file);
      OpenPdfTab(file, true);
      if (replace_output_edit_->text().trimmed().isEmpty()) {
        replace_output_edit_->setText(SuggestOutput(file, QStringLiteral("_replace"), QStringLiteral(".pdf")));
      }
    }
  });

  connect(pick_source, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择来源 PDF"),
                                                      DialogInitialPath(replace_source_edit_->text()),
                                                      QStringLiteral("PDF Files (*.pdf)"));
    if (!file.isEmpty()) {
      RememberDialogPath(file);
      replace_source_edit_->setText(file);
      OpenPdfTab(file, true);
    }
  });

  connect(pick_output, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("保存输出 PDF"),
                                                        DialogInitialPath(replace_output_edit_->text()),
                                                        QStringLiteral("PDF Files (*.pdf)"));
    if (!target.isEmpty()) {
      replace_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* target_row = new QHBoxLayout();
  target_row->addWidget(replace_target_edit_, 1);
  target_row->addWidget(pick_target);

  auto* source_row = new QHBoxLayout();
  source_row->addWidget(replace_source_edit_, 1);
  source_row->addWidget(pick_source);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(replace_output_edit_, 1);
  output_row->addWidget(pick_output);

  layout->addRow(QStringLiteral("从 Tab 选择目标"), replace_target_tab_combo_);
  layout->addRow(QStringLiteral("目标 PDF"), target_row);
  layout->addRow(QStringLiteral("从 Tab 选择来源"), replace_source_tab_combo_);
  layout->addRow(QStringLiteral("来源 PDF"), source_row);
  layout->addRow(QStringLiteral("输出 PDF"), output_row);
  layout->addRow(QStringLiteral("目标页 page"), replace_page_spin_);
  layout->addRow(QStringLiteral("来源页 source-page"), replace_source_page_spin_);
  auto* action_row = new QHBoxLayout();
  action_row->addWidget(replace_preview_button_);
  action_row->addWidget(replace_run_button_);
  layout->addRow(action_row);

  return page;
}

QWidget* MainWindow::BuildDocxPage() {
  auto* page = new QWidget(this);
  auto* layout = new QFormLayout(page);

  docx_input_tab_combo_ = new QComboBox(page);
  docx_input_edit_ = new QLineEdit(page);
  docx_output_edit_ = new QLineEdit(page);
  docx_dump_ir_edit_ = new QLineEdit(page);
  docx_no_images_check_ = new QCheckBox(QStringLiteral("No Images"), page);
  docx_anchored_check_ = new QCheckBox(QStringLiteral("Anchored Images"), page);
  docx_run_button_ = new QPushButton(QStringLiteral("执行"), page);

  auto* pick_input = new QPushButton(QStringLiteral("选择输入"), page);
  auto* pick_output = new QPushButton(QStringLiteral("选择输出"), page);
  auto* pick_dump = new QPushButton(QStringLiteral("选择 IR 输出"), page);

  connect(pick_input, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择输入 PDF"),
                                                      DialogInitialPath(docx_input_edit_->text()),
                                                      QStringLiteral("PDF Files (*.pdf)"));
    if (!file.isEmpty()) {
      RememberDialogPath(file);
      docx_input_edit_->setText(file);
      OpenPdfTab(file, true);
      if (docx_output_edit_->text().trimmed().isEmpty()) {
        docx_output_edit_->setText(SuggestOutput(file, QStringLiteral("_converted"), QStringLiteral(".docx")));
      }
      if (docx_dump_ir_edit_->text().trimmed().isEmpty()) {
        docx_dump_ir_edit_->setText(SuggestOutput(file, QStringLiteral("_ir_dump"), QStringLiteral(".json")));
      }
    }
  });

  connect(pick_output, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("保存 DOCX"),
                                                        DialogInitialPath(docx_output_edit_->text()),
                                                        QStringLiteral("DOCX Files (*.docx)"));
    if (!target.isEmpty()) {
      docx_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  connect(pick_dump, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("保存 IR JSON"),
                                                        DialogInitialPath(docx_dump_ir_edit_->text()),
                                                        QStringLiteral("JSON Files (*.json)"));
    if (!target.isEmpty()) {
      docx_dump_ir_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* input_row = new QHBoxLayout();
  input_row->addWidget(docx_input_edit_, 1);
  input_row->addWidget(pick_input);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(docx_output_edit_, 1);
  output_row->addWidget(pick_output);

  auto* dump_row = new QHBoxLayout();
  dump_row->addWidget(docx_dump_ir_edit_, 1);
  dump_row->addWidget(pick_dump);

  auto* check_row = new QHBoxLayout();
  check_row->addWidget(docx_no_images_check_);
  check_row->addWidget(docx_anchored_check_);
  check_row->addStretch(1);

  layout->addRow(QStringLiteral("从 Tab 选择输入"), docx_input_tab_combo_);
  layout->addRow(QStringLiteral("输入 PDF"), input_row);
  layout->addRow(QStringLiteral("输出 DOCX"), output_row);
  layout->addRow(QStringLiteral("可选 Dump IR"), dump_row);
  layout->addRow(QStringLiteral("选项"), check_row);
  layout->addRow(docx_run_button_);

  return page;
}

QWidget* MainWindow::BuildPreviewArea() {
  auto* panel = new QWidget(this);
  auto* root = new QVBoxLayout(panel);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  preview_tabs_ = new QTabWidget(panel);
  preview_tabs_->setTabsClosable(true);
  preview_tabs_->setMovable(true);
  root->addWidget(preview_tabs_, 1);

  auto* bar = statusBar();
  preview_current_label_ = new QLabel(QStringLiteral("文件位置: 未打开 PDF"), bar);
  preview_current_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  preview_current_label_->setMinimumWidth(360);
  preview_current_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  preview_prev_button_ = new QPushButton(QStringLiteral("上一页"), bar);
  preview_next_button_ = new QPushButton(QStringLiteral("下一页"), bar);
  preview_page_label_ = new QLabel(QStringLiteral("0 / 0"), bar);

  preview_zoom_out_button_ = new QPushButton(QStringLiteral("-"), bar);
  preview_zoom_label_ = new QLabel(QStringLiteral("100%"), bar);
  preview_zoom_in_button_ = new QPushButton(QStringLiteral("+"), bar);

  bar->addPermanentWidget(preview_current_label_, 1);
  bar->addPermanentWidget(preview_prev_button_);
  bar->addPermanentWidget(preview_page_label_);
  bar->addPermanentWidget(preview_next_button_);
  bar->addPermanentWidget(preview_zoom_out_button_);
  bar->addPermanentWidget(preview_zoom_label_);
  bar->addPermanentWidget(preview_zoom_in_button_);
  return panel;
}

void MainWindow::BindSignals() {
  connect(preview_tabs_, &QTabWidget::currentChanged, this, [this](int) {
    UpdatePreviewFooter();
    RefreshTabSelectors();
  });

  connect(preview_tabs_, &QTabWidget::tabCloseRequested, this, [this](int index) {
    RequestCloseTab(index);
  });

  auto* shortcut_prev = new QAction(this);
  shortcut_prev->setShortcut(QKeySequence(Qt::Key_PageUp));
  shortcut_prev->setShortcutContext(Qt::ApplicationShortcut);
  addAction(shortcut_prev);
  connect(shortcut_prev, &QAction::triggered, this, [this]() {
    if (preview_prev_button_ != nullptr) {
      preview_prev_button_->click();
    }
  });

  auto* shortcut_next = new QAction(this);
  shortcut_next->setShortcut(QKeySequence(Qt::Key_PageDown));
  shortcut_next->setShortcutContext(Qt::ApplicationShortcut);
  addAction(shortcut_next);
  connect(shortcut_next, &QAction::triggered, this, [this]() {
    if (preview_next_button_ != nullptr) {
      preview_next_button_->click();
    }
  });

  auto* shortcut_zoom_in = new QAction(this);
  shortcut_zoom_in->setShortcuts(QKeySequence::keyBindings(QKeySequence::ZoomIn));
  shortcut_zoom_in->setShortcutContext(Qt::ApplicationShortcut);
  addAction(shortcut_zoom_in);
  connect(shortcut_zoom_in, &QAction::triggered, this, [this]() {
    if (preview_zoom_in_button_ != nullptr) {
      preview_zoom_in_button_->click();
    }
  });

  auto* shortcut_zoom_out = new QAction(this);
  shortcut_zoom_out->setShortcuts(QKeySequence::keyBindings(QKeySequence::ZoomOut));
  shortcut_zoom_out->setShortcutContext(Qt::ApplicationShortcut);
  addAction(shortcut_zoom_out);
  connect(shortcut_zoom_out, &QAction::triggered, this, [this]() {
    if (preview_zoom_out_button_ != nullptr) {
      preview_zoom_out_button_->click();
    }
  });

  connect(preview_prev_button_, &QPushButton::clicked, this, [this]() {
    if (auto* tab = ActivePdfTab()) {
      tab->PrevPage();
      UpdatePreviewFooter();
    }
  });

  connect(preview_next_button_, &QPushButton::clicked, this, [this]() {
    if (auto* tab = ActivePdfTab()) {
      tab->NextPage();
      UpdatePreviewFooter();
    }
  });

  connect(preview_zoom_in_button_, &QPushButton::clicked, this, [this]() {
    if (auto* tab = ActivePdfTab()) {
      tab->ZoomIn();
      UpdatePreviewFooter();
    }
  });

  connect(preview_zoom_out_button_, &QPushButton::clicked, this, [this]() {
    if (auto* tab = ActivePdfTab()) {
      tab->ZoomOut();
      UpdatePreviewFooter();
    }
  });

  BindMergeSignals();
  BindDeleteSignals();
  BindInsertSignals();
  BindReplaceSignals();
  BindDocxSignals();
}

void MainWindow::BindMergeSignals() {
  connect(merge_preview_button_, &QPushButton::clicked, this, [this]() {
    const QStringList input_pdfs = ParseMultilinePaths(merge_inputs_edit_->toPlainText());
    if (input_pdfs.size() < 2) {
      AppendLog(QStringLiteral("合并预览至少需要 2 个输入 PDF。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kMerge;
    request.input_pdfs = input_pdfs;
    request.output_pdf = BuildPreviewOutputPath(request.type);
    RunTask(request, true);
  });

  connect(merge_run_button_, &QPushButton::clicked, this, [this]() {
    const QStringList input_pdfs = ParseMultilinePaths(merge_inputs_edit_->toPlainText());
    const QString output_pdf = merge_output_edit_->text().trimmed();
    if (input_pdfs.size() < 2) {
      AppendLog(QStringLiteral("合并任务至少需要 2 个输入 PDF。"));
      return;
    }
    if (output_pdf.isEmpty()) {
      AppendLog(QStringLiteral("请先选择合并输出文件。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kMerge;
    request.input_pdfs = input_pdfs;
    request.output_pdf = output_pdf;
    RunTask(request);
  });
}

void MainWindow::BindDeleteSignals() {
  connect(delete_input_tab_combo_, &QComboBox::currentIndexChanged, this, [this]() {
    const QString value = delete_input_tab_combo_->currentData().toString();
    if (!value.isEmpty()) {
      delete_input_edit_->setText(value);
    }
  });

  connect(delete_preview_button_, &QPushButton::clicked, this, [this]() {
    const QString input_pdf = delete_input_edit_->text().trimmed();
    if (input_pdf.isEmpty()) {
      AppendLog(QStringLiteral("删除页预览需要输入路径。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kDeletePage;
    request.input_pdf = input_pdf;
    request.output_pdf = BuildPreviewOutputPath(request.type);
    request.page = delete_page_spin_->value();
    RunTask(request, true);
  });

  connect(delete_run_button_, &QPushButton::clicked, this, [this]() {
    const QString input_pdf = delete_input_edit_->text().trimmed();
    const QString output_pdf = delete_output_edit_->text().trimmed();
    if (input_pdf.isEmpty() || output_pdf.isEmpty()) {
      AppendLog(QStringLiteral("删除页任务需要输入与输出路径。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kDeletePage;
    request.input_pdf = input_pdf;
    request.output_pdf = output_pdf;
    request.page = delete_page_spin_->value();
    RunTask(request);
  });
}

void MainWindow::BindInsertSignals() {
  connect(insert_target_tab_combo_, &QComboBox::currentIndexChanged, this, [this]() {
    const QString value = insert_target_tab_combo_->currentData().toString();
    if (!value.isEmpty()) {
      insert_target_edit_->setText(value);
    }
  });

  connect(insert_source_tab_combo_, &QComboBox::currentIndexChanged, this, [this]() {
    const QString value = insert_source_tab_combo_->currentData().toString();
    if (!value.isEmpty()) {
      insert_source_edit_->setText(value);
    }
  });

  connect(insert_preview_button_, &QPushButton::clicked, this, [this]() {
    const QString input_pdf = insert_target_edit_->text().trimmed();
    const QString source_pdf = insert_source_edit_->text().trimmed();
    if (input_pdf.isEmpty() || source_pdf.isEmpty()) {
      AppendLog(QStringLiteral("插入页预览需要目标和来源路径。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kInsertPage;
    request.input_pdf = input_pdf;
    request.source_pdf = source_pdf;
    request.output_pdf = BuildPreviewOutputPath(request.type);
    request.at = insert_at_spin_->value();
    request.source_page = insert_source_page_spin_->value();
    RunTask(request, true);
  });

  connect(insert_run_button_, &QPushButton::clicked, this, [this]() {
    const QString input_pdf = insert_target_edit_->text().trimmed();
    const QString source_pdf = insert_source_edit_->text().trimmed();
    const QString output_pdf = insert_output_edit_->text().trimmed();
    if (input_pdf.isEmpty() || source_pdf.isEmpty() || output_pdf.isEmpty()) {
      AppendLog(QStringLiteral("插入页任务需要目标、来源和输出路径。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kInsertPage;
    request.input_pdf = input_pdf;
    request.source_pdf = source_pdf;
    request.output_pdf = output_pdf;
    request.at = insert_at_spin_->value();
    request.source_page = insert_source_page_spin_->value();
    RunTask(request);
  });
}

void MainWindow::BindReplaceSignals() {
  connect(replace_target_tab_combo_, &QComboBox::currentIndexChanged, this, [this]() {
    const QString value = replace_target_tab_combo_->currentData().toString();
    if (!value.isEmpty()) {
      replace_target_edit_->setText(value);
    }
  });

  connect(replace_source_tab_combo_, &QComboBox::currentIndexChanged, this, [this]() {
    const QString value = replace_source_tab_combo_->currentData().toString();
    if (!value.isEmpty()) {
      replace_source_edit_->setText(value);
    }
  });

  connect(replace_preview_button_, &QPushButton::clicked, this, [this]() {
    const QString input_pdf = replace_target_edit_->text().trimmed();
    const QString source_pdf = replace_source_edit_->text().trimmed();
    if (input_pdf.isEmpty() || source_pdf.isEmpty()) {
      AppendLog(QStringLiteral("替换页预览需要目标和来源路径。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kReplacePage;
    request.input_pdf = input_pdf;
    request.source_pdf = source_pdf;
    request.output_pdf = BuildPreviewOutputPath(request.type);
    request.page = replace_page_spin_->value();
    request.source_page = replace_source_page_spin_->value();
    RunTask(request, true);
  });

  connect(replace_run_button_, &QPushButton::clicked, this, [this]() {
    const QString input_pdf = replace_target_edit_->text().trimmed();
    const QString source_pdf = replace_source_edit_->text().trimmed();
    const QString output_pdf = replace_output_edit_->text().trimmed();
    if (input_pdf.isEmpty() || source_pdf.isEmpty() || output_pdf.isEmpty()) {
      AppendLog(QStringLiteral("替换页任务需要目标、来源和输出路径。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kReplacePage;
    request.input_pdf = input_pdf;
    request.source_pdf = source_pdf;
    request.output_pdf = output_pdf;
    request.page = replace_page_spin_->value();
    request.source_page = replace_source_page_spin_->value();
    RunTask(request);
  });
}

void MainWindow::BindDocxSignals() {
  connect(docx_input_tab_combo_, &QComboBox::currentIndexChanged, this, [this]() {
    const QString value = docx_input_tab_combo_->currentData().toString();
    if (!value.isEmpty()) {
      docx_input_edit_->setText(value);
    }
  });

  connect(docx_run_button_, &QPushButton::clicked, this, [this]() {
    const QString input_pdf = docx_input_edit_->text().trimmed();
    const QString output_docx = docx_output_edit_->text().trimmed();
    if (input_pdf.isEmpty() || output_docx.isEmpty()) {
      AppendLog(QStringLiteral("PDF2DOCX 任务需要输入 PDF 与输出 DOCX。"));
      return;
    }

    core::TaskRequest request;
    request.type = core::TaskType::kPdfToDocx;
    request.input_pdf = input_pdf;
    request.output_docx = output_docx;
    request.dump_ir_path = docx_dump_ir_edit_->text().trimmed();
    request.no_images = docx_no_images_check_->isChecked();
    request.anchored_images = docx_anchored_check_->isChecked();
    RunTask(request);
  });
}

void MainWindow::AppendLog(const QString& message) {
  if (task_log_ == nullptr) {
    return;
  }

  const LogStyle style = ResolveLogStyle(message);
  const bool dark_ui = palette().color(QPalette::Window).lightness() < 128;
  const QString text_color = dark_ui ? style.dark_text_color : style.light_text_color;
  const QString html = QStringLiteral(
                           "<div style='margin:2px 0;'>"
                           "<span style='color:#9aa4b0;'>[%1]</span> "
                           "<span style='background:%2;color:#0f1115;border-radius:4px;padding:1px 6px;"
                           "font-size:11px;font-weight:600;'>%3</span> "
                           "<span style='color:%4;'>%5</span>"
                           "</div>")
                           .arg(NowTime(),
                                style.badge_color,
                                style.level,
                                text_color,
                                message.toHtmlEscaped());
  task_log_->append(html);
  if (auto* bar = task_log_->verticalScrollBar()) {
    bar->setValue(bar->maximum());
  }
}

void MainWindow::ApplyTheme(const QString& theme_mode) {
  QString normalized = theme_mode;
  if (normalized != QStringLiteral("dark") && normalized != QStringLiteral("light") &&
      normalized != QStringLiteral("system")) {
    normalized = QStringLiteral("system");
  }

  if (normalized == QStringLiteral("dark")) {
    qApp->setStyleSheet(
        QStringLiteral("QWidget { background: #1f1f1f; color: #e6e6e6; }"
                       "QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QListWidget, QTabWidget::pane {"
                       " background: #2a2a2a; color: #e6e6e6; border: 1px solid #3a3a3a; }"
                       "QPushButton { background: #2f4f7f; border: 1px solid #4a6ea9; padding: 4px 8px; }"
                       "QPushButton:disabled { background: #444; color: #888; }"));
  } else {
    qApp->setStyleSheet(QString());
  }
}

void MainWindow::OpenSettingsDialog() {
  qt_pdftools::ui::SettingsDialog dialog(this);
  dialog.SetValues(app_settings_);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  app_settings_ = dialog.Values();
  qt_pdftools::core::SaveAppSettings(settings_.get(), app_settings_);
  ApplyTheme(app_settings_.theme_mode);
  AppendLog(QStringLiteral("设置已更新。"));
}

void MainWindow::RefreshBackendStatus(bool write_log) {
  backend::ITaskBackend* backend = backend_registry_.Get(active_backend_id_);
  if (backend == nullptr) {
    if (write_log) {
      AppendLog(QStringLiteral("未找到可用 backend。"));
    }
    return;
  }

  const core::BackendStatus status = backend->GetStatus();
  if (write_log) {
    AppendLog(QStringLiteral("当前 backend: %1").arg(backend->label()));
    AppendLog(QStringLiteral("backend 状态: %1").arg(status.message));
  }
}

bool MainWindow::IsBackendReady() {
  backend::ITaskBackend* backend = backend_registry_.Get(active_backend_id_);
  if (backend == nullptr) {
    AppendLog(QStringLiteral("backend 不存在。"));
    return false;
  }
  const core::BackendStatus status = backend->GetStatus();
  if (!status.ready) {
    AppendLog(QStringLiteral("backend 未就绪: %1").arg(status.message));
    return false;
  }
  return true;
}

void MainWindow::OpenPdfTabs(const QStringList& paths, bool focus_last) {
  if (preview_tabs_ == nullptr) {
    return;
  }
  int opened = 0;
  for (int i = 0; i < paths.size(); ++i) {
    const bool focus = focus_last && (i == paths.size() - 1);
    const int before = preview_tabs_->count();
    OpenPdfTab(paths[i], focus);
    if (preview_tabs_->count() > before) {
      opened += 1;
    }
  }
  if (opened > 0) {
    AppendLog(QStringLiteral("File > Open: 打开 %1 个 PDF Tab").arg(opened));
  }
}

void MainWindow::OpenPdfTab(const QString& path,
                            bool focus,
                            const QString& custom_title,
                            bool is_preview_temp) {
  if (preview_tabs_ == nullptr) {
    return;
  }
  const QString normalized = path.trimmed();
  if (normalized.isEmpty()) {
    return;
  }

  for (int i = 0; i < preview_tabs_->count(); ++i) {
    auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(preview_tabs_->widget(i));
    if (tab != nullptr && tab->file_path() == normalized) {
      if (is_preview_temp) {
        tab->setProperty("is_preview_temp", true);
        tab->setProperty("preview_temp_path", normalized);
        tab->setProperty("save_path", QString());
        if (!custom_title.trimmed().isEmpty()) {
          preview_tabs_->setTabText(i, custom_title.trimmed());
        }
        SetTabDirty(i, true);
      }
      if (focus) {
        preview_tabs_->setCurrentIndex(i);
      }
      return;
    }
  }

  auto* tab = new qt_pdftools::ui::PdfTabView(preview_tabs_);
  QString error_message;
  if (!tab->LoadFile(normalized, &error_message)) {
    AppendLog(QStringLiteral("加载 PDF 失败: %1 (%2)").arg(normalized, error_message));
    tab->deleteLater();
    return;
  }

  const QString tab_title = custom_title.trimmed().isEmpty() ? Basename(normalized) : custom_title.trimmed();
  const int index = preview_tabs_->addTab(tab, tab_title);
  preview_tabs_->setTabToolTip(index,
                               is_preview_temp ? QStringLiteral("[Preview] %1").arg(normalized) : normalized);
  tab->setProperty("is_preview_temp", is_preview_temp);
  tab->setProperty("preview_temp_path", is_preview_temp ? normalized : QString());
  tab->setProperty("save_path", is_preview_temp ? QString() : normalized);
  tab->setProperty("is_dirty", is_preview_temp);
  EnsureTabCloseButton(index);
  RefreshTabCloseButton(index);
  SetTabDirty(index, is_preview_temp);
  if (focus) {
    preview_tabs_->setCurrentIndex(index);
  }

  connect(tab, &qt_pdftools::ui::PdfTabView::StateChanged, this, [this, tab]() {
    if (tab == ActivePdfTab()) {
      UpdatePreviewFooter();
    }
  });

  RefreshTabSelectors();
  UpdatePreviewFooter();
}

bool MainWindow::SaveTabByIndex(int index, bool force_save_as) {
  if (preview_tabs_ == nullptr || index < 0 || index >= preview_tabs_->count()) {
    return false;
  }

  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(preview_tabs_->widget(index));
  if (tab == nullptr) {
    return false;
  }

  const QString source_path = tab->file_path().trimmed();
  if (source_path.isEmpty() || !QFileInfo::exists(source_path)) {
    AppendLog(QStringLiteral("当前 Tab 文件不可保存: %1").arg(source_path));
    return false;
  }

  const bool is_preview_temp = tab->property("is_preview_temp").toBool();
  QString save_path = tab->property("save_path").toString().trimmed();
  if (save_path.isEmpty()) {
    save_path = source_path;
  }

  if (force_save_as || is_preview_temp || save_path.isEmpty()) {
    const QString selected = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("保存 PDF"),
                                                          DialogInitialPath(save_path),
                                                          QStringLiteral("PDF Files (*.pdf)"));
    if (selected.isEmpty()) {
      return false;
    }
    save_path = selected;
    RememberDialogPath(save_path);
  }

  const QString source_abs = QFileInfo(source_path).absoluteFilePath();
  const QString target_abs = QFileInfo(save_path).absoluteFilePath();

  if (source_abs != target_abs) {
    if (QFileInfo::exists(target_abs) && !QFile::remove(target_abs)) {
      AppendLog(QStringLiteral("保存失败：无法覆盖目标文件 %1").arg(target_abs));
      return false;
    }
    if (!QFile::copy(source_abs, target_abs)) {
      AppendLog(QStringLiteral("保存失败：复制文件失败 %1 -> %2").arg(source_abs, target_abs));
      return false;
    }
  }

  QString load_error;
  if (!tab->LoadFile(target_abs, &load_error)) {
    AppendLog(QStringLiteral("保存后重载失败: %1 (%2)").arg(target_abs, load_error));
    return false;
  }

  const QString old_preview_temp = tab->property("preview_temp_path").toString();
  if (!old_preview_temp.isEmpty() && old_preview_temp != target_abs && QFileInfo::exists(old_preview_temp)) {
    QFile::remove(old_preview_temp);
  }

  tab->setProperty("is_preview_temp", false);
  tab->setProperty("preview_temp_path", QString());
  tab->setProperty("save_path", target_abs);

  preview_tabs_->setTabText(index, Basename(target_abs));
  preview_tabs_->setTabToolTip(index, target_abs);
  SetTabDirty(index, false);
  UpdatePreviewFooter();

  AppendLog(QStringLiteral("已保存: %1").arg(target_abs));
  statusBar()->showMessage(QStringLiteral("已保存"), 3000);
  return true;
}

bool MainWindow::ConfirmCloseTab(int index) {
  if (preview_tabs_ == nullptr || index < 0 || index >= preview_tabs_->count()) {
    return false;
  }
  if (!IsTabDirty(index)) {
    return true;
  }

  const QString title = preview_tabs_->tabText(index);
  QMessageBox dialog(this);
  dialog.setWindowTitle(QStringLiteral("未保存的更改"));
  dialog.setIcon(QMessageBox::Warning);
  dialog.setText(QStringLiteral("“%1” 有未保存修改。").arg(title));
  dialog.setInformativeText(QStringLiteral("关闭前是否保存？"));

  QPushButton* save_button = dialog.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
  QPushButton* discard_button = dialog.addButton(QStringLiteral("丢弃修改"), QMessageBox::DestructiveRole);
  QPushButton* cancel_button = dialog.addButton(QStringLiteral("取消关闭"), QMessageBox::RejectRole);
  Q_UNUSED(discard_button)
  dialog.setDefaultButton(save_button);
  dialog.exec();

  if (dialog.clickedButton() == save_button) {
    return SaveTabByIndex(index, false);
  }
  if (dialog.clickedButton() == cancel_button) {
    return false;
  }
  return true;
}

bool MainWindow::RequestCloseTab(int index) {
  if (preview_tabs_ == nullptr || index < 0 || index >= preview_tabs_->count()) {
    return false;
  }
  if (!ConfirmCloseTab(index)) {
    return false;
  }

  QWidget* widget = preview_tabs_->widget(index);
  CleanupPreviewTempForWidget(widget);
  preview_tabs_->removeTab(index);
  if (widget != nullptr) {
    widget->deleteLater();
  }
  RefreshTabSelectors();
  UpdatePreviewFooter();
  return true;
}

void MainWindow::SetTabDirty(int index, bool dirty) {
  if (preview_tabs_ == nullptr || index < 0 || index >= preview_tabs_->count()) {
    return;
  }
  QWidget* widget = preview_tabs_->widget(index);
  if (widget == nullptr) {
    return;
  }
  widget->setProperty("is_dirty", dirty);
  RefreshTabCloseButton(index);
}

bool MainWindow::IsTabDirty(int index) const {
  if (preview_tabs_ == nullptr || index < 0 || index >= preview_tabs_->count()) {
    return false;
  }
  QWidget* widget = preview_tabs_->widget(index);
  return widget != nullptr ? widget->property("is_dirty").toBool() : false;
}

void MainWindow::EnsureTabCloseButton(int index) {
  if (preview_tabs_ == nullptr) {
    return;
  }
  auto* bar = preview_tabs_->tabBar();
  if (bar == nullptr || index < 0 || index >= bar->count()) {
    return;
  }

  auto* existing = qobject_cast<QToolButton*>(bar->tabButton(index, QTabBar::RightSide));
  if (existing != nullptr) {
    return;
  }

  auto* button = new QToolButton(bar);
  button->setAutoRaise(true);
  button->setFocusPolicy(Qt::NoFocus);
  button->setCursor(Qt::PointingHandCursor);
  button->setStyleSheet(QStringLiteral("QToolButton { border: none; padding: 0 4px; }"));
  connect(button, &QToolButton::clicked, this, [this, button]() {
    if (preview_tabs_ == nullptr) {
      return;
    }
    auto* tab_bar = preview_tabs_->tabBar();
    if (tab_bar == nullptr) {
      return;
    }
    for (int i = 0; i < tab_bar->count(); ++i) {
      if (tab_bar->tabButton(i, QTabBar::RightSide) == button) {
        RequestCloseTab(i);
        return;
      }
    }
  });

  bar->setTabButton(index, QTabBar::RightSide, button);
}

void MainWindow::RefreshTabCloseButton(int index) {
  if (preview_tabs_ == nullptr) {
    return;
  }
  auto* bar = preview_tabs_->tabBar();
  if (bar == nullptr || index < 0 || index >= bar->count()) {
    return;
  }

  EnsureTabCloseButton(index);
  auto* button = qobject_cast<QToolButton*>(bar->tabButton(index, QTabBar::RightSide));
  if (button == nullptr) {
    return;
  }

  const bool dirty = IsTabDirty(index);
  button->setText(dirty ? QStringLiteral("●") : QStringLiteral("×"));
  button->setToolTip(dirty ? QStringLiteral("有未保存修改") : QStringLiteral("关闭 Tab"));
}

void MainWindow::CleanupPreviewTempForWidget(QWidget* widget, bool write_log) {
  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr) {
    return;
  }

  const QString temp_path = tab->property("preview_temp_path").toString();
  if (temp_path.isEmpty()) {
    return;
  }

  QFileInfo info(temp_path);
  if (info.exists()) {
    if (QFile::remove(temp_path)) {
      if (write_log) {
        AppendLog(QStringLiteral("已清理预览临时文件: %1").arg(temp_path));
      }
    } else if (write_log) {
      AppendLog(QStringLiteral("清理预览临时文件失败: %1").arg(temp_path));
    }
  }

  tab->setProperty("is_preview_temp", false);
  tab->setProperty("preview_temp_path", QString());
}

void MainWindow::CloseCurrentPdfTab() {
  if (preview_tabs_ == nullptr) {
    return;
  }
  const int index = preview_tabs_->currentIndex();
  if (index < 0) {
    return;
  }
  RequestCloseTab(index);
}

void MainWindow::CloseAllPdfTabs() {
  if (preview_tabs_ == nullptr) {
    return;
  }
  while (preview_tabs_->count() > 0) {
    if (!RequestCloseTab(preview_tabs_->count() - 1)) {
      return;
    }
  }
}

qt_pdftools::ui::PdfTabView* MainWindow::ActivePdfTab() const {
  if (preview_tabs_ == nullptr) {
    return nullptr;
  }
  return qobject_cast<qt_pdftools::ui::PdfTabView*>(preview_tabs_->currentWidget());
}

QString MainWindow::ActivePdfPath() const {
  if (auto* tab = ActivePdfTab()) {
    return tab->file_path();
  }
  return {};
}

void MainWindow::UpdatePreviewFooter() {
  if (preview_current_label_ == nullptr || preview_page_label_ == nullptr || preview_zoom_label_ == nullptr) {
    return;
  }

  auto* tab = ActivePdfTab();
  if (tab == nullptr) {
    preview_current_label_->setText(QStringLiteral("文件位置: 未打开 PDF"));
    preview_page_label_->setText(QStringLiteral("0 / 0"));
    preview_zoom_label_->setText(QStringLiteral("100%"));
    return;
  }

  preview_current_label_->setText(QStringLiteral("文件位置: %1").arg(tab->file_path()));
  preview_page_label_->setText(QStringLiteral("%1 / %2").arg(tab->current_page()).arg(tab->page_count()));
  preview_zoom_label_->setText(QStringLiteral("%1%").arg(static_cast<int>(tab->zoom_factor() * 100.0)));
}

void MainWindow::RefreshTabSelectors() {
  if (preview_tabs_ == nullptr || merge_tab_select_ == nullptr || delete_input_tab_combo_ == nullptr ||
      insert_target_tab_combo_ == nullptr || insert_source_tab_combo_ == nullptr ||
      replace_target_tab_combo_ == nullptr || replace_source_tab_combo_ == nullptr ||
      docx_input_tab_combo_ == nullptr) {
    return;
  }

  QStringList paths;
  for (int i = 0; i < preview_tabs_->count(); ++i) {
    auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(preview_tabs_->widget(i));
    if (tab != nullptr) {
      paths << tab->file_path();
    }
  }

  const QString active_path = ActivePdfPath();

  QStringList selected_merge_paths;
  for (QListWidgetItem* item : merge_tab_select_->selectedItems()) {
    selected_merge_paths << item->data(Qt::UserRole).toString();
  }
  merge_tab_select_->clear();
  for (const QString& path : paths) {
    auto* item = new QListWidgetItem(Basename(path), merge_tab_select_);
    item->setData(Qt::UserRole, path);
    if (selected_merge_paths.contains(path)) {
      item->setSelected(true);
    }
  }

  FillComboWithPaths(delete_input_tab_combo_, paths, delete_input_edit_->text().trimmed(), active_path);
  FillComboWithPaths(insert_target_tab_combo_, paths, insert_target_edit_->text().trimmed(), active_path);
  FillComboWithPaths(insert_source_tab_combo_, paths, insert_source_edit_->text().trimmed(), active_path);
  FillComboWithPaths(replace_target_tab_combo_, paths, replace_target_edit_->text().trimmed(), active_path);
  FillComboWithPaths(replace_source_tab_combo_, paths, replace_source_edit_->text().trimmed(), active_path);
  FillComboWithPaths(docx_input_tab_combo_, paths, docx_input_edit_->text().trimmed(), active_path);
}

void MainWindow::RunTask(const qt_pdftools::core::TaskRequest& request, bool preview_only) {
  if (running_task_) {
    AppendLog(QStringLiteral("已有任务执行中，请等待当前任务结束。"));
    return;
  }

  if (!IsBackendReady()) {
    return;
  }

  StartTaskExecution(request, preview_only);
}

void MainWindow::StartTaskExecution(const qt_pdftools::core::TaskRequest& request, bool preview_only) {
  backend::ITaskBackend* backend = backend_registry_.Get(active_backend_id_);
  if (backend == nullptr) {
    AppendLog(QStringLiteral("backend 不存在，任务取消。"));
    return;
  }

  running_task_ = true;
  active_task_preview_ = preview_only;
  active_task_id_ = next_task_id_++;
  if (task_watcher_ != nullptr) {
    task_watcher_->deleteLater();
    task_watcher_ = nullptr;
  }

  const QString mode = preview_only ? QStringLiteral("预览") : QStringLiteral("执行");
  AppendLog(QStringLiteral("任务 #%1 开始：%2[%3]（backend=%4）")
                .arg(active_task_id_)
                .arg(core::TaskTypeToDisplayName(request.type), mode, backend->id()));

  for (QPushButton* button :
       {merge_run_button_,
        merge_preview_button_,
        delete_run_button_,
        delete_preview_button_,
        insert_run_button_,
        insert_preview_button_,
        replace_run_button_,
        replace_preview_button_,
        docx_run_button_}) {
    if (button != nullptr) {
      button->setEnabled(false);
    }
  }

  task_watcher_ = new QFutureWatcher<qt_pdftools::core::TaskResult>(this);
  connect(task_watcher_, &QFutureWatcher<qt_pdftools::core::TaskResult>::finished, this, [this, request]() {
    const core::TaskResult result = task_watcher_->result();
    HandleTaskFinished(request, result);
    task_watcher_->deleteLater();
    task_watcher_ = nullptr;
  });

  task_watcher_->setFuture(QtConcurrent::run([backend, request]() { return backend->RunTask(request); }));
}

void MainWindow::HandleTaskFinished(const qt_pdftools::core::TaskRequest& request,
                                    const qt_pdftools::core::TaskResult& result) {
  running_task_ = false;
  const bool preview_only = active_task_preview_;
  active_task_preview_ = false;
  const int task_id = active_task_id_;
  active_task_id_ = 0;
  for (QPushButton* button :
       {merge_run_button_,
        merge_preview_button_,
        delete_run_button_,
        delete_preview_button_,
        insert_run_button_,
        insert_preview_button_,
        replace_run_button_,
        replace_preview_button_,
        docx_run_button_}) {
    if (button != nullptr) {
      button->setEnabled(true);
    }
  }

  if (!result.args.isEmpty()) {
    AppendLog(QStringLiteral("命令参数: %1").arg(result.args.join(' ')));
  }
  if (!result.stdout_text.isEmpty()) {
    AppendLog(QStringLiteral("stdout: %1").arg(result.stdout_text));
  }
  if (!result.stderr_text.isEmpty()) {
    AppendLog(QStringLiteral("stderr: %1").arg(result.stderr_text));
  }

  if (result.ok) {
    if (preview_only) {
      AppendLog(QStringLiteral("任务 #%1 预览已生成：%2").arg(task_id).arg(result.summary));
    } else {
      AppendLog(QStringLiteral("任务 #%1 成功：%2").arg(task_id).arg(result.summary));
    }
    if (!result.output_path.isEmpty()) {
      AppendLog(QStringLiteral("输出文件: %1").arg(result.output_path));
    }
    if (!result.output_path.isEmpty() && request.type != core::TaskType::kPdfToDocx) {
      if (preview_only) {
        const QString preview_title =
            QStringLiteral("[Preview] %1 #%2").arg(core::TaskTypeToString(request.type)).arg(task_id);
        OpenPdfTab(result.output_path, true, preview_title, true);
      } else {
        OpenPdfTab(result.output_path, true);
      }
    }
    statusBar()->showMessage(preview_only ? QStringLiteral("预览已生成") : QStringLiteral("任务成功"), 4000);
    return;
  }

  AppendLog(QStringLiteral("任务 #%1 %2失败(code=%3)")
                .arg(task_id)
                .arg(preview_only ? QStringLiteral("预览") : QString())
                .arg(result.error.code));
  if (!result.error.context.isEmpty()) {
    AppendLog(QStringLiteral("失败上下文: %1").arg(result.error.context));
  }
  statusBar()->showMessage(QStringLiteral("任务失败"), 5000);
}

QString MainWindow::BuildPreviewOutputPath(qt_pdftools::core::TaskType type) const {
  QString op_name = QStringLiteral("op");
  switch (type) {
    case core::TaskType::kMerge:
      op_name = QStringLiteral("merge");
      break;
    case core::TaskType::kDeletePage:
      op_name = QStringLiteral("delete");
      break;
    case core::TaskType::kInsertPage:
      op_name = QStringLiteral("insert");
      break;
    case core::TaskType::kReplacePage:
      op_name = QStringLiteral("replace");
      break;
    case core::TaskType::kPdfToDocx:
      op_name = QStringLiteral("pdf2docx");
      break;
  }

  QDir dir(QDir::tempPath() + QStringLiteral("/qt_pdftools_preview"));
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }
  const qint64 ts = QDateTime::currentMSecsSinceEpoch();
  return dir.filePath(QStringLiteral("preview_%1_%2.pdf").arg(op_name).arg(ts));
}

QString MainWindow::DialogInitialPath(const QString& preferred) const {
  const QString preferred_path = preferred.trimmed();
  if (!preferred_path.isEmpty()) {
    return preferred_path;
  }

  const QString remembered_dir = app_settings_.last_open_dir.trimmed();
  if (!remembered_dir.isEmpty()) {
    return remembered_dir;
  }
  return QDir::homePath();
}

void MainWindow::RememberDialogPath(const QString& selected_path) {
  const QString normalized = selected_path.trimmed();
  if (normalized.isEmpty()) {
    return;
  }

  QFileInfo info(normalized);
  const QString dir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
  if (dir.isEmpty() || dir == app_settings_.last_open_dir) {
    return;
  }

  app_settings_.last_open_dir = dir;
  qt_pdftools::core::SaveAppSettings(settings_.get(), app_settings_);
}

QStringList MainWindow::ParseMultilinePaths(const QString& raw_text) const {
  QStringList values;
  const QStringList lines = raw_text.split(QRegularExpression(QStringLiteral("\\r?\\n")),
                                           Qt::SkipEmptyParts);
  for (const QString& line : lines) {
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      values << trimmed;
    }
  }
  return values;
}

QString MainWindow::SuggestOutput(const QString& input_path,
                                  const QString& suffix,
                                  const QString& extension) const {
  QFileInfo info(input_path);
  const QString base = info.completeBaseName();
  const QString dir = info.absolutePath();
  return QStringLiteral("%1/%2%3%4").arg(dir, base, suffix, extension);
}

}  // namespace qt_pdftools::app
