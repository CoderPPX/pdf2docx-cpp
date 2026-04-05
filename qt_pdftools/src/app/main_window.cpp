#include "qt_pdftools/app/main_window.hpp"

#include <algorithm>
#include <utility>

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
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSaveFile>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
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

#if QT_PDFTOOLS_HAS_QTPDF
#include <QtPdf/qpdfdocument.h>
#endif

namespace qt_pdftools::app {
namespace {

QString NowTime() {
  return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

QString Basename(const QString& path) {
  return QFileInfo(path).fileName();
}

QPushButton* CreateFileDialogButton(QWidget* parent, const QString& tooltip) {
  auto* button = new QPushButton(parent);
  button->setIcon(parent->style()->standardIcon(QStyle::SP_DirOpenIcon));
  button->setToolTip(tooltip);
  button->setFixedWidth(32);
  return button;
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
  combo->addItem(QStringLiteral("-- 请选择已打开文件 --"), QString());
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
  if (lower.contains(QStringLiteral("开始任务")) || lower.contains(QStringLiteral("任务 #")) ||
      lower.contains(QStringLiteral("开始：")) || lower.contains(QStringLiteral("运行")) ||
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
  AppendLog(QStringLiteral("界面已初始化。"));
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
  setWindowTitle(QStringLiteral("Qt PDF工具"));
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
  auto* file_menu = menuBar()->addMenu(QStringLiteral("文件"));

  auto* open_pdf_action = file_menu->addAction(QStringLiteral("打开 PDF 到新标签页..."));
  auto* save_action = file_menu->addAction(QStringLiteral("保存"));
  auto* save_as_action = file_menu->addAction(QStringLiteral("另存为..."));
  file_menu->addSeparator();
  auto* close_current_action = file_menu->addAction(QStringLiteral("关闭当前标签页"));
  auto* close_all_action = file_menu->addAction(QStringLiteral("关闭全部标签页"));
  auto* close_preview_action = file_menu->addAction(QStringLiteral("关闭全部预览标签页"));
  file_menu->addSeparator();
  auto* settings_action = file_menu->addAction(QStringLiteral("设置..."));
  file_menu->addSeparator();
  auto* exit_action = file_menu->addAction(QStringLiteral("退出"));

  open_pdf_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Open));
  save_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Save));
  save_as_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::SaveAs));
  close_current_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Close));
  close_all_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
  close_preview_action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_W));
  settings_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Preferences));
  exit_action->setShortcuts(QKeySequence::keyBindings(QKeySequence::Quit));

  connect(open_pdf_action, &QAction::triggered, this, [this]() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择 PDF 文件"),
        DialogInitialPath(),
        QStringLiteral("PDF 文件 (*.pdf)"));
    if (!files.isEmpty()) {
      RememberDialogPath(files.first());
    }
    OpenPdfTabs(files, true);
  });

  connect(save_action, &QAction::triggered, this, [this]() {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可保存的标签页。"));
      return;
    }
    SaveTabByIndex(preview_tabs_->currentIndex(), false);
  });

  connect(save_as_action, &QAction::triggered, this, [this]() {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可另存为的标签页。"));
      return;
    }
    SaveTabByIndex(preview_tabs_->currentIndex(), true);
  });

  connect(close_current_action, &QAction::triggered, this, &MainWindow::CloseCurrentPdfTab);
  connect(close_all_action, &QAction::triggered, this, &MainWindow::CloseAllPdfTabs);
  connect(close_preview_action, &QAction::triggered, this, [this]() {
    if (preview_tabs_ == nullptr) {
      return;
    }
    int closed = 0;
    for (int i = preview_tabs_->count() - 1; i >= 0; --i) {
      QWidget* widget = preview_tabs_->widget(i);
      if (widget == nullptr || !widget->property("is_preview_temp").toBool()) {
        continue;
      }
      if (!RequestCloseTab(i)) {
        break;
      }
      closed += 1;
    }
    AppendLog(QStringLiteral("已关闭 %1 个预览标签页。").arg(closed));
  });
  connect(settings_action, &QAction::triggered, this, &MainWindow::OpenSettingsDialog);
  connect(exit_action, &QAction::triggered, this, &QWidget::close);

  auto* edit_menu = menuBar()->addMenu(QStringLiteral("编辑"));
  undo_action_ = edit_menu->addAction(QStringLiteral("撤销"));
  redo_action_ = edit_menu->addAction(QStringLiteral("重做"));
  undo_action_->setShortcuts(QKeySequence::keyBindings(QKeySequence::Undo));
  redo_action_->setShortcuts(QKeySequence::keyBindings(QKeySequence::Redo));
  undo_action_->setEnabled(false);
  redo_action_->setEnabled(false);

  connect(undo_action_, &QAction::triggered, this, [this]() {
    UndoCurrentTab();
  });
  connect(redo_action_, &QAction::triggered, this, [this]() {
    RedoCurrentTab();
  });
}

QWidget* MainWindow::BuildSidebar() {
  auto* sidebar = new QWidget(this);
  auto* root = new QVBoxLayout(sidebar);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  auto* tools_scroll = new QScrollArea(sidebar);
  tools_scroll->setWidgetResizable(true);
  auto* tools_host = new QWidget(tools_scroll);
  auto* tools_layout = new QVBoxLayout(tools_host);
  tools_layout->setContentsMargins(0, 0, 0, 0);
  tools_layout->setSpacing(6);

  const auto add_section = [tools_layout](const QString& title, QWidget* body, bool expanded_by_default) {
    auto* section = new QWidget(tools_layout->parentWidget());
    auto* section_layout = new QVBoxLayout(section);
    section_layout->setContentsMargins(0, 0, 0, 0);
    section_layout->setSpacing(2);

    auto* header = new QToolButton(section);
    header->setText(title);
    header->setCheckable(true);
    header->setChecked(expanded_by_default);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setArrowType(expanded_by_default ? Qt::DownArrow : Qt::RightArrow);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    header->setStyleSheet(QStringLiteral(
        "QToolButton { text-align: left; padding: 6px 8px; border: 1px solid palette(mid); border-radius: 4px; }"));

    body->setParent(section);
    body->setVisible(expanded_by_default);

    section_layout->addWidget(header);
    section_layout->addWidget(body);

    connect(header, &QToolButton::toggled, body, [header, body](bool expanded) {
      header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
      body->setVisible(expanded);
    });

    tools_layout->addWidget(section);
  };

  add_section(QStringLiteral("编辑当前PDF"), BuildEditCurrentPage(), true);
  add_section(QStringLiteral("提取图片"), BuildExtractImagesPage(), false);
  add_section(QStringLiteral("PDF合并"), BuildMergePage(), false);
  add_section(QStringLiteral("删除页"), BuildDeletePage(), false);
  add_section(QStringLiteral("插入页"), BuildInsertPage(), false);
  add_section(QStringLiteral("替换页"), BuildReplacePage(), false);
  add_section(QStringLiteral("PDF转DOCX"), BuildDocxPage(), false);
  add_section(QStringLiteral("批注与书签"), BuildAnnotationBookmarkPage(), false);
  tools_layout->addStretch(1);

  tools_scroll->setWidget(tools_host);
  root->addWidget(tools_scroll, 3);

  auto* log_group = new QGroupBox(QStringLiteral("日志"), sidebar);
  auto* log_layout = new QVBoxLayout(log_group);
  task_log_ = new QTextEdit(log_group);
  task_log_->setReadOnly(true);
  task_log_->document()->setMaximumBlockCount(2000);
  task_log_->setMinimumHeight(220);
  task_log_->setPlaceholderText(QStringLiteral("日志输出将在这里显示。"));
  log_layout->addWidget(task_log_, 1);
  root->addWidget(log_group, 2);

  return sidebar;
}

QWidget* MainWindow::BuildMergePage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  merge_tab_select_ = new QListWidget(page);
  merge_tab_select_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  merge_tab_select_->setMinimumHeight(92);

  auto* use_tabs_button = new QPushButton(QStringLiteral("从打开的文件填充输入"), page);
  connect(use_tabs_button, &QPushButton::clicked, this, [this]() {
    QStringList selected_paths;
    const QList<QListWidgetItem*> items = merge_tab_select_->selectedItems();
    for (QListWidgetItem* item : items) {
      selected_paths << item->data(Qt::UserRole).toString();
    }
    if (selected_paths.isEmpty()) {
      AppendLog(QStringLiteral("未选择任何打开的文件。"));
      return;
    }
    merge_inputs_edit_->setPlainText(selected_paths.join(QStringLiteral("\n")));
    AppendLog(QStringLiteral("已用 %1 个打开的文件填充合并输入。").arg(selected_paths.size()));
  });

  merge_inputs_edit_ = new QTextEdit(page);
  merge_inputs_edit_->setPlaceholderText(QStringLiteral("每行一个 PDF 路径"));
  merge_inputs_edit_->setMinimumHeight(100);

  auto* output_row = new QHBoxLayout();
  merge_output_edit_ = new QLineEdit(page);
  auto* output_browse = CreateFileDialogButton(page, QStringLiteral("选择输出 PDF"));
  output_row->addWidget(output_browse);
  output_row->addWidget(merge_output_edit_, 1);
  connect(output_browse, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存合并 PDF"),
        DialogInitialPath(merge_output_edit_->text()),
        QStringLiteral("PDF 文件 (*.pdf)"));
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

  layout->addWidget(merge_tab_select_);
  layout->addWidget(use_tabs_button);
  layout->addWidget(merge_inputs_edit_);
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

  auto* pick_input = CreateFileDialogButton(page, QStringLiteral("选择输入 PDF"));
  auto* pick_output = CreateFileDialogButton(page, QStringLiteral("选择输出 PDF"));
  delete_run_button_ = new QPushButton(QStringLiteral("执行"), page);
  delete_preview_button_ = new QPushButton(QStringLiteral("预览"), page);

  connect(pick_input, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择输入 PDF"),
                                                      DialogInitialPath(delete_input_edit_->text()),
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
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
                                                        QStringLiteral("PDF 文件 (*.pdf)"));
    if (!target.isEmpty()) {
      delete_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* input_row = new QHBoxLayout();
  input_row->addWidget(pick_input);
  input_row->addWidget(delete_input_edit_, 1);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(pick_output);
  output_row->addWidget(delete_output_edit_, 1);

  layout->addRow(QStringLiteral("从打开的文件选择输入"), delete_input_tab_combo_);
  layout->addRow(QStringLiteral("输入 PDF"), input_row);
  layout->addRow(QStringLiteral("输出 PDF"), output_row);
  layout->addRow(QStringLiteral("删除页（从1开始）"), delete_page_spin_);
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

  auto* pick_target = CreateFileDialogButton(page, QStringLiteral("选择目标 PDF"));
  auto* pick_source = CreateFileDialogButton(page, QStringLiteral("选择来源 PDF"));
  auto* pick_output = CreateFileDialogButton(page, QStringLiteral("选择输出 PDF"));
  insert_run_button_ = new QPushButton(QStringLiteral("执行"), page);
  insert_preview_button_ = new QPushButton(QStringLiteral("预览"), page);

  connect(pick_target, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择目标 PDF"),
                                                      DialogInitialPath(insert_target_edit_->text()),
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
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
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
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
                                                        QStringLiteral("PDF 文件 (*.pdf)"));
    if (!target.isEmpty()) {
      insert_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* target_row = new QHBoxLayout();
  target_row->addWidget(pick_target);
  target_row->addWidget(insert_target_edit_, 1);

  auto* source_row = new QHBoxLayout();
  source_row->addWidget(pick_source);
  source_row->addWidget(insert_source_edit_, 1);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(pick_output);
  output_row->addWidget(insert_output_edit_, 1);

  layout->addRow(QStringLiteral("从打开的文件选择目标"), insert_target_tab_combo_);
  layout->addRow(QStringLiteral("目标 PDF"), target_row);
  layout->addRow(QStringLiteral("从打开的文件选择来源"), insert_source_tab_combo_);
  layout->addRow(QStringLiteral("来源 PDF"), source_row);
  layout->addRow(QStringLiteral("输出 PDF"), output_row);
  layout->addRow(QStringLiteral("插入位置"), insert_at_spin_);
  layout->addRow(QStringLiteral("来源页"), insert_source_page_spin_);
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

  auto* pick_target = CreateFileDialogButton(page, QStringLiteral("选择目标 PDF"));
  auto* pick_source = CreateFileDialogButton(page, QStringLiteral("选择来源 PDF"));
  auto* pick_output = CreateFileDialogButton(page, QStringLiteral("选择输出 PDF"));
  replace_run_button_ = new QPushButton(QStringLiteral("执行"), page);
  replace_preview_button_ = new QPushButton(QStringLiteral("预览"), page);

  connect(pick_target, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择目标 PDF"),
                                                      DialogInitialPath(replace_target_edit_->text()),
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
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
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
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
                                                        QStringLiteral("PDF 文件 (*.pdf)"));
    if (!target.isEmpty()) {
      replace_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* target_row = new QHBoxLayout();
  target_row->addWidget(pick_target);
  target_row->addWidget(replace_target_edit_, 1);

  auto* source_row = new QHBoxLayout();
  source_row->addWidget(pick_source);
  source_row->addWidget(replace_source_edit_, 1);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(pick_output);
  output_row->addWidget(replace_output_edit_, 1);

  layout->addRow(QStringLiteral("从打开的文件选择目标"), replace_target_tab_combo_);
  layout->addRow(QStringLiteral("目标 PDF"), target_row);
  layout->addRow(QStringLiteral("从打开的文件选择来源"), replace_source_tab_combo_);
  layout->addRow(QStringLiteral("来源 PDF"), source_row);
  layout->addRow(QStringLiteral("输出 PDF"), output_row);
  layout->addRow(QStringLiteral("目标页"), replace_page_spin_);
  layout->addRow(QStringLiteral("来源页"), replace_source_page_spin_);
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
  docx_no_images_check_ = new QCheckBox(QStringLiteral("不导出图片"), page);
  docx_anchored_check_ = new QCheckBox(QStringLiteral("锚定图片"), page);
  docx_run_button_ = new QPushButton(QStringLiteral("执行"), page);

  auto* pick_input = CreateFileDialogButton(page, QStringLiteral("选择输入 PDF"));
  auto* pick_output = CreateFileDialogButton(page, QStringLiteral("选择输出 DOCX"));
  auto* pick_dump = CreateFileDialogButton(page, QStringLiteral("选择 IR 输出路径"));

  connect(pick_input, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择输入 PDF"),
                                                      DialogInitialPath(docx_input_edit_->text()),
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
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
                                                        QStringLiteral("DOCX 文件 (*.docx)"));
    if (!target.isEmpty()) {
      docx_output_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  connect(pick_dump, &QPushButton::clicked, this, [this]() {
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("保存 IR JSON"),
                                                        DialogInitialPath(docx_dump_ir_edit_->text()),
                                                        QStringLiteral("JSON 文件 (*.json)"));
    if (!target.isEmpty()) {
      docx_dump_ir_edit_->setText(target);
      RememberDialogPath(target);
    }
  });

  auto* input_row = new QHBoxLayout();
  input_row->addWidget(pick_input);
  input_row->addWidget(docx_input_edit_, 1);

  auto* output_row = new QHBoxLayout();
  output_row->addWidget(pick_output);
  output_row->addWidget(docx_output_edit_, 1);

  auto* dump_row = new QHBoxLayout();
  dump_row->addWidget(pick_dump);
  dump_row->addWidget(docx_dump_ir_edit_, 1);

  auto* check_row = new QHBoxLayout();
  check_row->addWidget(docx_no_images_check_);
  check_row->addWidget(docx_anchored_check_);
  check_row->addStretch(1);

  layout->addRow(QStringLiteral("从打开的文件选择输入"), docx_input_tab_combo_);
  layout->addRow(QStringLiteral("输入 PDF"), input_row);
  layout->addRow(QStringLiteral("输出 DOCX"), output_row);
  layout->addRow(QStringLiteral("可选 IR 导出"), dump_row);
  layout->addRow(QStringLiteral("选项"), check_row);
  layout->addRow(docx_run_button_);

  return page;
}

QWidget* MainWindow::BuildEditCurrentPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 4, 8, 8);
  layout->setSpacing(8);

  auto* merge_group = new QGroupBox(QStringLiteral("合并进当前PDF"), page);
  auto* merge_layout = new QVBoxLayout(merge_group);
  auto* merge_source_row = new QHBoxLayout();
  edit_current_merge_source_edit_ = new QLineEdit(merge_group);
  edit_current_merge_source_edit_->setPlaceholderText(QStringLiteral("选择要合并的外部PDF"));
  auto* merge_pick_button = CreateFileDialogButton(merge_group, QStringLiteral("选择待合并 PDF"));
  merge_source_row->addWidget(merge_pick_button);
  merge_source_row->addWidget(edit_current_merge_source_edit_, 1);
  auto* merge_action_row = new QHBoxLayout();
  edit_current_merge_append_radio_ = new QRadioButton(QStringLiteral("追加到末尾"), merge_group);
  edit_current_merge_prepend_radio_ = new QRadioButton(QStringLiteral("在开头插入"), merge_group);
  edit_current_merge_append_radio_->setChecked(true);
  edit_current_merge_apply_button_ = new QPushButton(QStringLiteral("合并"), merge_group);
  merge_action_row->addWidget(edit_current_merge_append_radio_);
  merge_action_row->addWidget(edit_current_merge_prepend_radio_);
  merge_action_row->addStretch(1);
  merge_action_row->addWidget(edit_current_merge_apply_button_);
  merge_layout->addLayout(merge_source_row);
  merge_layout->addLayout(merge_action_row);
  layout->addWidget(merge_group);

  auto* delete_group = new QGroupBox(QStringLiteral("删除页"), page);
  auto* delete_layout = new QVBoxLayout(delete_group);
  auto* delete_row = new QHBoxLayout();
  edit_current_delete_page_spin_ = new QSpinBox(delete_group);
  edit_current_delete_page_spin_->setRange(1, 999999);
  edit_current_delete_page_spin_->setValue(1);
  edit_current_delete_apply_button_ = new QPushButton(QStringLiteral("删除"), delete_group);
  delete_row->addWidget(new QLabel(QStringLiteral("页码"), delete_group));
  delete_row->addWidget(edit_current_delete_page_spin_);
  delete_row->addWidget(edit_current_delete_apply_button_);
  delete_row->addStretch(1);
  delete_layout->addLayout(delete_row);
  layout->addWidget(delete_group);

  auto* swap_group = new QGroupBox(QStringLiteral("交换页"), page);
  auto* swap_layout = new QFormLayout(swap_group);
  edit_current_swap_page_a_spin_ = new QSpinBox(swap_group);
  edit_current_swap_page_a_spin_->setRange(1, 999999);
  edit_current_swap_page_a_spin_->setValue(1);
  edit_current_swap_page_b_spin_ = new QSpinBox(swap_group);
  edit_current_swap_page_b_spin_->setRange(1, 999999);
  edit_current_swap_page_b_spin_->setValue(2);
  edit_current_swap_apply_button_ = new QPushButton(QStringLiteral("交换"), swap_group);
  swap_layout->addRow(QStringLiteral("页 A"), edit_current_swap_page_a_spin_);
  swap_layout->addRow(QStringLiteral("页 B"), edit_current_swap_page_b_spin_);
  swap_layout->addRow(edit_current_swap_apply_button_);
  layout->addWidget(swap_group);

  connect(merge_pick_button, &QPushButton::clicked, this, [this]() {
    const QString file = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择待合并 PDF"),
                                                      DialogInitialPath(edit_current_merge_source_edit_->text()),
                                                      QStringLiteral("PDF 文件 (*.pdf)"));
    if (!file.isEmpty()) {
      edit_current_merge_source_edit_->setText(file);
      RememberDialogPath(file);
    }
  });

  layout->addStretch(1);
  return page;
}

QWidget* MainWindow::BuildExtractImagesPage() {
  auto* page = new QWidget(this);
  auto* layout = new QFormLayout(page);
  edit_current_extract_from_spin_ = new QSpinBox(page);
  edit_current_extract_to_spin_ = new QSpinBox(page);
  edit_current_extract_from_spin_->setRange(1, 999999);
  edit_current_extract_to_spin_->setRange(1, 999999);
  edit_current_extract_from_spin_->setValue(1);
  edit_current_extract_to_spin_->setValue(1);
  edit_current_extract_output_dir_edit_ = new QLineEdit(page);
  auto* extract_pick_dir = CreateFileDialogButton(page, QStringLiteral("选择输出目录"));
  auto* output_row = new QHBoxLayout();
  output_row->addWidget(extract_pick_dir);
  output_row->addWidget(edit_current_extract_output_dir_edit_, 1);
  edit_current_extract_run_button_ = new QPushButton(QStringLiteral("提取内嵌图像"), page);

  layout->addRow(QStringLiteral("起始页"), edit_current_extract_from_spin_);
  layout->addRow(QStringLiteral("结束页"), edit_current_extract_to_spin_);
  layout->addRow(QStringLiteral("输出目录"), output_row);
  layout->addRow(edit_current_extract_run_button_);

  connect(extract_pick_dir, &QPushButton::clicked, this, [this]() {
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择页面图像导出目录"),
        DialogInitialPath(edit_current_extract_output_dir_edit_->text()));
    if (!dir.isEmpty()) {
      edit_current_extract_output_dir_edit_->setText(dir);
      RememberDialogPath(dir);
    }
  });
  return page;
}

QWidget* MainWindow::BuildAnnotationBookmarkPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 4, 8, 8);
  layout->setSpacing(8);

  auto* annotation_group = new QGroupBox(QStringLiteral("画笔与标记"), page);
  auto* annotation_layout = new QVBoxLayout(annotation_group);
  auto* tool_row = new QHBoxLayout();
  annotation_pen_button_ = new QPushButton(QStringLiteral("画笔"), annotation_group);
  annotation_marker_button_ = new QPushButton(QStringLiteral("荧光笔"), annotation_group);
  annotation_eraser_button_ = new QPushButton(QStringLiteral("橡皮擦"), annotation_group);
  annotation_pen_button_->setCheckable(true);
  annotation_marker_button_->setCheckable(true);
  annotation_eraser_button_->setCheckable(true);
  tool_row->addWidget(annotation_pen_button_);
  tool_row->addWidget(annotation_marker_button_);
  tool_row->addWidget(annotation_eraser_button_);
  auto* action_row = new QHBoxLayout();
  annotation_undo_button_ = new QPushButton(QStringLiteral("撤销笔迹"), annotation_group);
  annotation_clear_button_ = new QPushButton(QStringLiteral("清空当前页"), annotation_group);
  action_row->addWidget(annotation_undo_button_);
  action_row->addWidget(annotation_clear_button_);
  action_row->addStretch(1);
  tool_row->addStretch(1);
  annotation_layout->addLayout(tool_row);
  annotation_layout->addLayout(action_row);
  layout->addWidget(annotation_group);

  auto* bookmark_group = new QGroupBox(QStringLiteral("书签"), page);
  auto* bookmark_layout = new QVBoxLayout(bookmark_group);
  auto* bookmark_row = new QHBoxLayout();
  bookmark_add_button_ = new QPushButton(QStringLiteral("添加当前页书签"), bookmark_group);
  bookmark_jump_button_ = new QPushButton(QStringLiteral("跳转"), bookmark_group);
  bookmark_rename_button_ = new QPushButton(QStringLiteral("重命名"), bookmark_group);
  bookmark_delete_button_ = new QPushButton(QStringLiteral("删除"), bookmark_group);
  bookmark_row->addWidget(bookmark_add_button_);
  bookmark_row->addWidget(bookmark_jump_button_);
  bookmark_row->addWidget(bookmark_rename_button_);
  bookmark_row->addWidget(bookmark_delete_button_);
  bookmark_row->addStretch(1);
  bookmark_list_ = new QListWidget(bookmark_group);
  bookmark_list_->setMinimumHeight(120);
  bookmark_layout->addLayout(bookmark_row);
  bookmark_layout->addWidget(bookmark_list_);
  layout->addWidget(bookmark_group);

  layout->addStretch(1);
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
  preview_current_label_ = new QLabel(QStringLiteral("文件位置: 未打开PDF文件"), bar);
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
    RefreshBookmarkPanel();
    UpdateUndoRedoActions();
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
  BindEditCurrentSignals();
  BindExtractImagesSignals();
  BindAnnotationBookmarkSignals();
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
      AppendLog(QStringLiteral("PDF转DOCX 任务需要输入 PDF 与输出 DOCX。"));
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

void MainWindow::BindEditCurrentSignals() {
  connect(edit_current_merge_apply_button_, &QPushButton::clicked, this, [this]() {
    if (ActivePdfTab() == nullptr || preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可编辑的PDF标签页。"));
      return;
    }
    const QString source_pdf = edit_current_merge_source_edit_->text().trimmed();
    const QString current_pdf = ActivePdfPath().trimmed();
    if (source_pdf.isEmpty() || current_pdf.isEmpty()) {
      AppendLog(QStringLiteral("应用合并需要当前标签页与来源PDF。"));
      return;
    }

    const bool prepend = (edit_current_merge_prepend_radio_ != nullptr &&
                          edit_current_merge_prepend_radio_->isChecked());
    SessionOp op;
    op.type = prepend ? SessionOpType::kMergePrepend : SessionOpType::kMergeAppend;
    op.source_pdf = source_pdf;
    ApplyEditOpToCurrentTab(op);
  });

  connect(edit_current_delete_apply_button_, &QPushButton::clicked, this, [this]() {
    if (ActivePdfTab() == nullptr || preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可编辑的PDF标签页。"));
      return;
    }
    SessionOp op;
    op.type = SessionOpType::kDeletePage;
    op.page = edit_current_delete_page_spin_->value();
    ApplyEditOpToCurrentTab(op);
  });

  connect(edit_current_swap_apply_button_, &QPushButton::clicked, this, [this]() {
    if (ActivePdfTab() == nullptr || preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可编辑的PDF标签页。"));
      return;
    }
    const int page_a = edit_current_swap_page_a_spin_->value();
    const int page_b = edit_current_swap_page_b_spin_->value();
    if (page_a == page_b) {
      AppendLog(QStringLiteral("交换页应用要求 A/B 不同。"));
      return;
    }
    SessionOp op;
    op.type = SessionOpType::kSwapPages;
    op.page = page_a;
    op.page_b = page_b;
    ApplyEditOpToCurrentTab(op);
  });
}

void MainWindow::BindExtractImagesSignals() {
  connect(edit_current_extract_run_button_, &QPushButton::clicked, this, [this]() {
    auto* tab = ActivePdfTab();
    if (tab == nullptr) {
      AppendLog(QStringLiteral("当前没有可导出图像的PDF标签页。"));
      return;
    }

    const int total_pages = std::max(1, tab->page_count());
    int from_page = std::clamp(edit_current_extract_from_spin_->value(), 1, total_pages);
    int to_page = std::clamp(edit_current_extract_to_spin_->value(), 1, total_pages);
    if (from_page > to_page) {
      std::swap(from_page, to_page);
    }

    QString output_dir = edit_current_extract_output_dir_edit_->text().trimmed();
    if (output_dir.isEmpty()) {
      const QFileInfo info(tab->file_path());
      output_dir = QStringLiteral("%1/%2_pages")
                       .arg(info.absolutePath(),
                            info.completeBaseName());
      edit_current_extract_output_dir_edit_->setText(output_dir);
    }

    QDir dir(output_dir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
      AppendLog(QStringLiteral("创建输出目录失败: %1").arg(output_dir));
      return;
    }
    RememberDialogPath(output_dir);

    core::TaskRequest request;
    request.type = core::TaskType::kExtractImages;
    request.input_pdf = tab->file_path();
    request.output_dir = output_dir;
    request.page = from_page;
    request.page_b = to_page;
    RunTask(request, false);
  });
}

void MainWindow::BindAnnotationBookmarkSignals() {
  if (bookmark_list_ == nullptr || bookmark_add_button_ == nullptr || bookmark_jump_button_ == nullptr ||
      bookmark_rename_button_ == nullptr || bookmark_delete_button_ == nullptr ||
      annotation_pen_button_ == nullptr || annotation_marker_button_ == nullptr ||
      annotation_eraser_button_ == nullptr || annotation_undo_button_ == nullptr ||
      annotation_clear_button_ == nullptr) {
    return;
  }

  const auto set_tool = [this](qt_pdftools::ui::PdfTabView::InkTool tool, const QString& tool_name) {
    auto* tab = ActivePdfTab();
    if (tab == nullptr) {
      return;
    }
    tab->SetInkTool(tool);
    annotation_pen_button_->setChecked(tool == qt_pdftools::ui::PdfTabView::InkTool::kPen);
    annotation_marker_button_->setChecked(tool == qt_pdftools::ui::PdfTabView::InkTool::kMarker);
    annotation_eraser_button_->setChecked(tool == qt_pdftools::ui::PdfTabView::InkTool::kEraser);
    if (!tool_name.isEmpty()) {
      AppendLog(QStringLiteral("已切换标记工具: %1").arg(tool_name));
    }
  };

  connect(annotation_pen_button_, &QPushButton::clicked, this, [set_tool](bool checked) {
    set_tool(checked ? qt_pdftools::ui::PdfTabView::InkTool::kPen : qt_pdftools::ui::PdfTabView::InkTool::kNone,
             checked ? QStringLiteral("画笔") : QString());
  });
  connect(annotation_marker_button_, &QPushButton::clicked, this, [set_tool](bool checked) {
    set_tool(checked ? qt_pdftools::ui::PdfTabView::InkTool::kMarker : qt_pdftools::ui::PdfTabView::InkTool::kNone,
             checked ? QStringLiteral("荧光笔") : QString());
  });
  connect(annotation_eraser_button_, &QPushButton::clicked, this, [set_tool](bool checked) {
    set_tool(checked ? qt_pdftools::ui::PdfTabView::InkTool::kEraser : qt_pdftools::ui::PdfTabView::InkTool::kNone,
             checked ? QStringLiteral("橡皮擦") : QString());
  });

  connect(annotation_undo_button_, &QPushButton::clicked, this, [this]() {
    auto* tab = ActivePdfTab();
    if (tab == nullptr) {
      return;
    }
    if (tab->UndoInkOnCurrentPage()) {
      AppendLog(QStringLiteral("已撤销当前页笔迹。"));
    }
  });

  connect(annotation_clear_button_, &QPushButton::clicked, this, [this]() {
    auto* tab = ActivePdfTab();
    if (tab == nullptr) {
      return;
    }
    tab->ClearInkOnCurrentPage();
    AppendLog(QStringLiteral("已清空当前页笔迹。"));
  });

  connect(bookmark_add_button_, &QPushButton::clicked, this, [this]() {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("当前没有可添加书签的标签页。"));
      return;
    }
    QWidget* widget = preview_tabs_->currentWidget();
    auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
    if (widget == nullptr || tab == nullptr) {
      return;
    }

    QVariantList entries = bookmarks_by_tab_.value(widget);
    const int current_page = std::max(1, tab->current_page());
    QVariantMap entry;
    entry.insert(QStringLiteral("title"),
                 QStringLiteral("书签 %1 (P%2)").arg(entries.size() + 1).arg(current_page));
    entry.insert(QStringLiteral("page"), current_page);
    entries.push_back(entry);
    bookmarks_by_tab_[widget] = entries;

    widget->setProperty("bookmark_dirty", true);
    SetTabDirty(preview_tabs_->currentIndex(), true);
    RefreshBookmarkPanel();
    AppendLog(QStringLiteral("已添加书签：第 %1 页").arg(current_page));
  });

  connect(bookmark_jump_button_, &QPushButton::clicked, this, [this]() {
    auto* tab = ActivePdfTab();
    if (tab == nullptr || bookmark_list_ == nullptr || bookmark_list_->currentItem() == nullptr) {
      return;
    }
    const int page = bookmark_list_->currentItem()->data(Qt::UserRole).toInt();
    tab->SetCurrentPage(page);
    UpdatePreviewFooter();
    AppendLog(QStringLiteral("已跳转到书签页：%1").arg(page));
  });

  connect(bookmark_rename_button_, &QPushButton::clicked, this, [this]() {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0 || bookmark_list_ == nullptr ||
        bookmark_list_->currentRow() < 0) {
      return;
    }
    QWidget* widget = preview_tabs_->currentWidget();
    const int row = bookmark_list_->currentRow();
    QVariantList entries = bookmarks_by_tab_.value(widget);
    if (row < 0 || row >= entries.size()) {
      return;
    }
    QVariantMap entry = entries[row].toMap();
    const QString old_title = entry.value(QStringLiteral("title")).toString();
    bool ok = false;
    const QString new_title =
        QInputDialog::getText(this,
                              QStringLiteral("重命名书签"),
                              QStringLiteral("书签标题"),
                              QLineEdit::Normal,
                              old_title,
                              &ok)
            .trimmed();
    if (!ok || new_title.isEmpty() || new_title == old_title) {
      return;
    }
    entry.insert(QStringLiteral("title"), new_title);
    entries[row] = entry;
    bookmarks_by_tab_[widget] = entries;
    widget->setProperty("bookmark_dirty", true);
    SetTabDirty(preview_tabs_->currentIndex(), true);
    RefreshBookmarkPanel();
    bookmark_list_->setCurrentRow(row);
    AppendLog(QStringLiteral("已重命名书签：%1").arg(new_title));
  });

  connect(bookmark_delete_button_, &QPushButton::clicked, this, [this]() {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0 || bookmark_list_ == nullptr ||
        bookmark_list_->currentRow() < 0) {
      return;
    }
    QWidget* widget = preview_tabs_->currentWidget();
    const int row = bookmark_list_->currentRow();
    QVariantList entries = bookmarks_by_tab_.value(widget);
    if (row < 0 || row >= entries.size()) {
      return;
    }
    entries.removeAt(row);
    bookmarks_by_tab_[widget] = entries;
    widget->setProperty("bookmark_dirty", true);
    SetTabDirty(preview_tabs_->currentIndex(), true);
    RefreshBookmarkPanel();
    AppendLog(QStringLiteral("已删除书签。"));
  });

  connect(bookmark_list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
    if (bookmark_jump_button_ != nullptr) {
      bookmark_jump_button_->click();
    }
  });

  connect(bookmark_list_, &QListWidget::itemSelectionChanged, this, [this]() {
    const bool has_selection = bookmark_list_ != nullptr && bookmark_list_->currentRow() >= 0;
    if (bookmark_jump_button_ != nullptr) {
      bookmark_jump_button_->setEnabled(has_selection);
    }
    if (bookmark_rename_button_ != nullptr) {
      bookmark_rename_button_->setEnabled(has_selection);
    }
    if (bookmark_delete_button_ != nullptr) {
      bookmark_delete_button_->setEnabled(has_selection);
    }
  });

  RefreshBookmarkPanel();
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
                           "<span style='background:%2;color:#f8fafc;border-radius:4px;padding:1px 6px;"
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
    AppendLog(QStringLiteral("文件>打开：打开 %1 个PDF标签页").arg(opened));
  }
}

void MainWindow::OpenPdfTab(const QString& path,
                            bool focus,
                            const QString& custom_title,
                            bool is_preview_temp) {
  if (preview_tabs_ == nullptr) {
    return;
  }
  const QString normalized = QFileInfo(path.trimmed()).absoluteFilePath();
  if (normalized.isEmpty()) {
    return;
  }

  for (int i = 0; i < preview_tabs_->count(); ++i) {
    auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(preview_tabs_->widget(i));
    if (tab != nullptr && tab->file_path() == normalized) {
      DocumentSession* session = EnsureSessionForTab(tab);
      if (is_preview_temp) {
        tab->setProperty("is_preview_temp", true);
        tab->setProperty("preview_temp_path", normalized);
        tab->setProperty("save_path", QString());
        if (session != nullptr) {
          session->dirty = true;
          session->save_path.clear();
        }
        if (!custom_title.trimmed().isEmpty()) {
          preview_tabs_->setTabText(i, custom_title.trimmed());
        }
        SetTabDirty(i, true);
      } else if (session != nullptr) {
        session->save_path = normalized;
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
                               is_preview_temp ? QStringLiteral("[预览] %1").arg(normalized) : normalized);
  tab->setProperty("is_preview_temp", is_preview_temp);
  tab->setProperty("preview_temp_path", is_preview_temp ? normalized : QString());
  tab->setProperty("save_path", is_preview_temp ? QString() : normalized);
  tab->setProperty("is_dirty", is_preview_temp);
  tab->setProperty("bookmark_dirty", false);
  undo_stack_by_tab_[tab].clear();
  redo_stack_by_tab_[tab].clear();
  sessions_by_tab_.remove(tab);
  DocumentSession* session = EnsureSessionForTab(tab);
  if (session != nullptr) {
    session->base_pdf_path = normalized;
    session->save_path = is_preview_temp ? QString() : normalized;
    session->dirty = is_preview_temp;
    session->applied_ops.clear();
    session->redo_ops.clear();
    session->logical_pages.clear();
    const int total_pages = std::max(0, tab->page_count());
    session->logical_pages.reserve(total_pages);
    for (int page = 1; page <= total_pages; ++page) {
      session->logical_pages.push_back(SessionPageRef{normalized, page});
    }
  }
  LoadBookmarksForWidget(tab, is_preview_temp ? QString() : normalized);
  LoadInkForWidget(tab, is_preview_temp ? QString() : normalized);
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

  connect(tab, &qt_pdftools::ui::PdfTabView::InkEdited, this, [this, tab]() {
    if (preview_tabs_ == nullptr) {
      return;
    }
    const int tab_index = preview_tabs_->indexOf(tab);
    if (tab_index >= 0) {
      SetTabDirty(tab_index, true);
      statusBar()->showMessage(QStringLiteral("笔迹已更新（未保存）"), 2500);
    }
  });

  RefreshTabSelectors();
  RefreshBookmarkPanel();
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
    AppendLog(QStringLiteral("当前标签页文件不可保存: %1").arg(source_path));
    return false;
  }

  DocumentSession* session = EnsureSessionForTab(tab);
  QString save_path = tab->property("save_path").toString().trimmed();
  if (save_path.isEmpty() && session != nullptr) {
    save_path = session->save_path.trimmed();
  }
  if (save_path.isEmpty()) {
    save_path = source_path;
  }

  if (force_save_as || save_path.isEmpty()) {
    const QString selected = QFileDialog::getSaveFileName(this,
                                                          QStringLiteral("保存 PDF"),
                                                          DialogInitialPath(save_path),
                                                          QStringLiteral("PDF 文件 (*.pdf)"));
    if (selected.isEmpty()) {
      return false;
    }
    save_path = selected;
    RememberDialogPath(save_path);
  }

  const QString source_abs = QFileInfo(source_path).absoluteFilePath();
  const QString target_abs = QFileInfo(save_path).absoluteFilePath();
  const QString old_preview_temp = tab->property("preview_temp_path").toString();
  const bool needs_materialize = session != nullptr && !session->applied_ops.isEmpty();

  if (needs_materialize) {
    if (!IsBackendReady()) {
      return false;
    }
    AppendLog(QStringLiteral("开始保存会话编辑，共 %1 步...").arg(session->applied_ops.size()));
    QString materialize_error;
    if (!MaterializeSessionForTab(tab, target_abs, &materialize_error)) {
      AppendLog(QStringLiteral("保存失败：会话物化失败: %1").arg(materialize_error));
      return false;
    }
  } else {
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
  }

  const bool referenced_by_history =
      undo_stack_by_tab_[tab].contains(old_preview_temp) || redo_stack_by_tab_[tab].contains(old_preview_temp);
  if (!old_preview_temp.isEmpty() && old_preview_temp != target_abs && QFileInfo::exists(old_preview_temp) &&
      !referenced_by_history) {
    QFile::remove(old_preview_temp);
  }

  if (!SaveBookmarksForWidget(tab, target_abs)) {
    AppendLog(QStringLiteral("保存失败：书签 sidecar 写入失败。"));
    return false;
  }
  if (!SaveInkForWidget(tab, target_abs)) {
    AppendLog(QStringLiteral("保存失败：墨迹 sidecar 写入失败。"));
    return false;
  }

  tab->setProperty("is_preview_temp", false);
  tab->setProperty("preview_temp_path", QString());
  tab->setProperty("save_path", target_abs);
  if (session != nullptr) {
    session->base_pdf_path = target_abs;
    session->save_path = target_abs;
    session->dirty = false;
    session->applied_ops.clear();
    session->redo_ops.clear();
    session->logical_pages.clear();
    const int total_pages = std::max(0, tab->page_count());
    session->logical_pages.reserve(total_pages);
    for (int page = 1; page <= total_pages; ++page) {
      session->logical_pages.push_back(SessionPageRef{target_abs, page});
    }
  }
  pdf_page_count_cache_.remove(target_abs);
  undo_stack_by_tab_[tab].clear();
  redo_stack_by_tab_[tab].clear();

  preview_tabs_->setTabText(index, Basename(target_abs));
  preview_tabs_->setTabToolTip(index, target_abs);
  SetTabDirty(index, false);
  UpdatePreviewFooter();
  UpdateUndoRedoActions();

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
  UpdateUndoRedoActions();
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
  auto session_it = sessions_by_tab_.find(widget);
  if (session_it != sessions_by_tab_.end()) {
    session_it->dirty = dirty || !session_it->applied_ops.isEmpty();
  }
  RefreshTabCloseButton(index);
  UpdateUndoRedoActions();
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
  button->setToolTip(dirty ? QStringLiteral("有未保存修改") : QStringLiteral("关闭标签页"));
}

bool MainWindow::UndoCurrentTab() {
  if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
    return false;
  }
  QWidget* widget = preview_tabs_->currentWidget();
  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr) {
    return false;
  }

  DocumentSession* session = EnsureSessionForTab(widget);
  if (session != nullptr && !session->applied_ops.isEmpty()) {
    const SessionOp undone_op = session->applied_ops.takeLast();
    session->redo_ops.push_back(undone_op);
    QString rebuild_error;
    if (!RebuildSessionLogicalPages(session, &rebuild_error)) {
      session->applied_ops.push_back(undone_op);
      session->redo_ops.removeLast();
      AppendLog(QStringLiteral("Undo 失败: %1").arg(rebuild_error));
      return false;
    }
    QString preview_error;
    if (!RenderSessionPreviewForTab(tab, &preview_error)) {
      session->redo_ops.removeLast();
      session->applied_ops.push_back(undone_op);
      RebuildSessionLogicalPages(session, nullptr);
      AppendLog(QStringLiteral("Undo 失败: 预览刷新失败: %1").arg(preview_error));
      return false;
    }
    const bool other_dirty = widget->property("bookmark_dirty").toBool() || tab->HasAnyInk();
    SetTabDirty(preview_tabs_->currentIndex(), other_dirty || !session->applied_ops.isEmpty());
    UpdatePreviewFooter();
    UpdateUndoRedoActions();
    AppendLog(QStringLiteral("Undo 完成（会话）：%1").arg(DescribeSessionOp(undone_op)));
    return true;
  }

  QStringList& undo_stack = undo_stack_by_tab_[widget];
  if (undo_stack.isEmpty()) {
    return false;
  }

  const QString target_snapshot = undo_stack.takeLast();
  const QString current_path = tab->file_path();
  QString load_error;
  if (!tab->LoadFile(target_snapshot, &load_error)) {
    AppendLog(QStringLiteral("Undo 失败: %1").arg(load_error));
    undo_stack.push_back(target_snapshot);
    return false;
  }

  redo_stack_by_tab_[widget].push_back(current_path);
  tab->setProperty("is_preview_temp", true);
  tab->setProperty("preview_temp_path", target_snapshot);
  if (tab->property("save_path").toString().trimmed().isEmpty()) {
    tab->setProperty("save_path", current_path);
  }
  SetTabDirty(preview_tabs_->currentIndex(), true);
  UpdatePreviewFooter();
  UpdateUndoRedoActions();
  AppendLog(QStringLiteral("Undo 完成。"));
  return true;
}

bool MainWindow::RedoCurrentTab() {
  if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
    return false;
  }
  QWidget* widget = preview_tabs_->currentWidget();
  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr) {
    return false;
  }

  DocumentSession* session = EnsureSessionForTab(widget);
  if (session != nullptr && !session->redo_ops.isEmpty()) {
    const SessionOp redo_op = session->redo_ops.takeLast();
    session->applied_ops.push_back(redo_op);
    QString rebuild_error;
    if (!RebuildSessionLogicalPages(session, &rebuild_error)) {
      session->applied_ops.removeLast();
      session->redo_ops.push_back(redo_op);
      AppendLog(QStringLiteral("Redo 失败: %1").arg(rebuild_error));
      return false;
    }
    QString preview_error;
    if (!RenderSessionPreviewForTab(tab, &preview_error)) {
      session->applied_ops.removeLast();
      session->redo_ops.push_back(redo_op);
      RebuildSessionLogicalPages(session, nullptr);
      AppendLog(QStringLiteral("Redo 失败: 预览刷新失败: %1").arg(preview_error));
      return false;
    }
    SetTabDirty(preview_tabs_->currentIndex(), true);
    UpdatePreviewFooter();
    UpdateUndoRedoActions();
    AppendLog(QStringLiteral("Redo 完成（会话）：%1").arg(DescribeSessionOp(redo_op)));
    return true;
  }

  QStringList& redo_stack = redo_stack_by_tab_[widget];
  if (redo_stack.isEmpty()) {
    return false;
  }

  const QString target_snapshot = redo_stack.takeLast();
  const QString current_path = tab->file_path();
  QString load_error;
  if (!tab->LoadFile(target_snapshot, &load_error)) {
    AppendLog(QStringLiteral("Redo 失败: %1").arg(load_error));
    redo_stack.push_back(target_snapshot);
    return false;
  }

  undo_stack_by_tab_[widget].push_back(current_path);
  tab->setProperty("is_preview_temp", true);
  tab->setProperty("preview_temp_path", target_snapshot);
  if (tab->property("save_path").toString().trimmed().isEmpty()) {
    tab->setProperty("save_path", current_path);
  }
  SetTabDirty(preview_tabs_->currentIndex(), true);
  UpdatePreviewFooter();
  UpdateUndoRedoActions();
  AppendLog(QStringLiteral("Redo 完成。"));
  return true;
}

void MainWindow::UpdateUndoRedoActions() {
  if (undo_action_ == nullptr || redo_action_ == nullptr || preview_tabs_ == nullptr ||
      preview_tabs_->currentIndex() < 0) {
    if (undo_action_ != nullptr) {
      undo_action_->setEnabled(false);
    }
    if (redo_action_ != nullptr) {
      redo_action_->setEnabled(false);
    }
    return;
  }

  QWidget* widget = preview_tabs_->currentWidget();
  bool can_undo = false;
  bool can_redo = false;
  if (widget != nullptr) {
    const auto session_it = sessions_by_tab_.constFind(widget);
    if (session_it != sessions_by_tab_.constEnd()) {
      can_undo = can_undo || !session_it->applied_ops.isEmpty();
      can_redo = can_redo || !session_it->redo_ops.isEmpty();
    }
    can_undo = can_undo || !undo_stack_by_tab_[widget].isEmpty();
    can_redo = can_redo || !redo_stack_by_tab_[widget].isEmpty();
  }
  undo_action_->setEnabled(can_undo);
  redo_action_->setEnabled(can_redo);
}

bool MainWindow::IsManagedTempPath(const QString& path) const {
  const QString normalized = QFileInfo(path).absoluteFilePath();
  const QString temp_root = QDir::tempPath();
  return normalized.startsWith(temp_root + QStringLiteral("/qt_pdftools_preview/")) ||
         normalized.startsWith(temp_root + QStringLiteral("/qt_pdftools_swap/")) ||
         normalized.startsWith(temp_root + QStringLiteral("/qt_pdftools_session/"));
}

void MainWindow::CleanupPreviewTempForWidget(QWidget* widget, bool write_log) {
  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr || widget == nullptr) {
    return;
  }

  const QString temp_path = tab->property("preview_temp_path").toString();
  if (temp_path.isEmpty()) {
    // continue to cleanup undo/redo snapshots below
  } else {
    QFileInfo info(temp_path);
    if (info.exists() && IsManagedTempPath(temp_path)) {
      if (QFile::remove(temp_path)) {
        if (write_log) {
          AppendLog(QStringLiteral("已清理预览临时文件: %1").arg(temp_path));
        }
      } else if (write_log) {
        AppendLog(QStringLiteral("清理预览临时文件失败: %1").arg(temp_path));
      }
    }
  }

  tab->setProperty("is_preview_temp", false);
  tab->setProperty("preview_temp_path", QString());

  auto cleanup_stack_files = [this, write_log](const QStringList& stack) {
    for (const QString& path : stack) {
      if (!IsManagedTempPath(path)) {
        continue;
      }
      if (QFileInfo::exists(path) && !QFile::remove(path) && write_log) {
        AppendLog(QStringLiteral("清理历史临时文件失败: %1").arg(path));
      }
    }
  };
  cleanup_stack_files(undo_stack_by_tab_.value(widget));
  cleanup_stack_files(redo_stack_by_tab_.value(widget));
  undo_stack_by_tab_.remove(widget);
  redo_stack_by_tab_.remove(widget);
  bookmarks_by_tab_.remove(widget);
  sessions_by_tab_.remove(widget);
  UpdateUndoRedoActions();
  RefreshBookmarkPanel();
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

  auto update_edit_ranges = [this](int max_page) {
    const int clamped_max = std::max(1, max_page);
    if (edit_current_delete_page_spin_ != nullptr) {
      edit_current_delete_page_spin_->setMaximum(clamped_max);
    }
    if (edit_current_swap_page_a_spin_ != nullptr) {
      edit_current_swap_page_a_spin_->setMaximum(clamped_max);
    }
    if (edit_current_swap_page_b_spin_ != nullptr) {
      edit_current_swap_page_b_spin_->setMaximum(clamped_max);
    }
    if (edit_current_extract_from_spin_ != nullptr) {
      edit_current_extract_from_spin_->setMaximum(clamped_max);
    }
    if (edit_current_extract_to_spin_ != nullptr) {
      edit_current_extract_to_spin_->setMaximum(clamped_max);
    }
  };

  auto* tab = ActivePdfTab();
  if (tab == nullptr) {
    preview_current_label_->setText(QStringLiteral("文件位置: 未打开PDF文件"));
    preview_page_label_->setText(QStringLiteral("0 / 0"));
    preview_zoom_label_->setText(QStringLiteral("100%"));
    update_edit_ranges(1);
    return;
  }

  QWidget* current_widget = preview_tabs_ != nullptr ? preview_tabs_->currentWidget() : nullptr;
  const DocumentSession* session = SessionForTab(current_widget);
  const int total_pages = (session != nullptr && !session->logical_pages.isEmpty())
                              ? session->logical_pages.size()
                              : tab->page_count();
  const int current_page = total_pages > 0 ? std::clamp(tab->current_page(), 1, total_pages) : 0;
  QString display_path = tab->file_path();
  if (current_widget != nullptr) {
    QString canonical_path;
    if (session != nullptr) {
      canonical_path = session->save_path.trimmed();
      if (canonical_path.isEmpty()) {
        canonical_path = session->base_pdf_path.trimmed();
      }
    }
    if (canonical_path.isEmpty()) {
      canonical_path = current_widget->property("save_path").toString().trimmed();
    }
    const bool is_preview_temp = current_widget->property("is_preview_temp").toBool();
    const bool is_dirty = current_widget->property("is_dirty").toBool();
    if (!canonical_path.isEmpty() &&
        (is_preview_temp || is_dirty || IsManagedTempPath(tab->file_path()))) {
      display_path = canonical_path;
    }
  }

  preview_current_label_->setText(QStringLiteral("文件位置: %1").arg(display_path));
  preview_page_label_->setText(QStringLiteral("%1 / %2").arg(current_page).arg(total_pages));
  preview_zoom_label_->setText(QStringLiteral("%1%").arg(static_cast<int>(tab->zoom_factor() * 100.0)));
  update_edit_ranges(total_pages);
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

void MainWindow::RefreshBookmarkPanel() {
  if (bookmark_list_ == nullptr || bookmark_add_button_ == nullptr || bookmark_jump_button_ == nullptr ||
      bookmark_rename_button_ == nullptr || bookmark_delete_button_ == nullptr ||
      annotation_pen_button_ == nullptr || annotation_marker_button_ == nullptr ||
      annotation_eraser_button_ == nullptr || annotation_undo_button_ == nullptr ||
      annotation_clear_button_ == nullptr) {
    return;
  }

  bookmark_list_->blockSignals(true);
  bookmark_list_->clear();

  QWidget* widget = preview_tabs_ != nullptr ? preview_tabs_->currentWidget() : nullptr;
  const bool has_tab = widget != nullptr;
  bookmark_add_button_->setEnabled(has_tab);

  if (has_tab) {
    const QVariantList entries = bookmarks_by_tab_.value(widget);
    for (const QVariant& value : entries) {
      const QVariantMap entry = value.toMap();
      const QString title = entry.value(QStringLiteral("title")).toString().trimmed();
      const int page = std::max(1, entry.value(QStringLiteral("page")).toInt());
      auto* item = new QListWidgetItem(
          QStringLiteral("[P%1] %2").arg(page).arg(title.isEmpty() ? QStringLiteral("未命名书签") : title),
          bookmark_list_);
      item->setData(Qt::UserRole, page);
    }
  }

  bookmark_list_->blockSignals(false);
  const bool has_selection = bookmark_list_->currentRow() >= 0;
  bookmark_jump_button_->setEnabled(has_tab && has_selection);
  bookmark_rename_button_->setEnabled(has_tab && has_selection);
  bookmark_delete_button_->setEnabled(has_tab && has_selection);

  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  const bool can_annotate = tab != nullptr;
  annotation_pen_button_->setEnabled(can_annotate);
  annotation_marker_button_->setEnabled(can_annotate);
  annotation_eraser_button_->setEnabled(can_annotate);
  annotation_undo_button_->setEnabled(can_annotate);
  annotation_clear_button_->setEnabled(can_annotate);

  if (tab == nullptr) {
    annotation_pen_button_->setChecked(false);
    annotation_marker_button_->setChecked(false);
    annotation_eraser_button_->setChecked(false);
    return;
  }
  annotation_pen_button_->setChecked(tab->ink_tool() == qt_pdftools::ui::PdfTabView::InkTool::kPen);
  annotation_marker_button_->setChecked(tab->ink_tool() == qt_pdftools::ui::PdfTabView::InkTool::kMarker);
  annotation_eraser_button_->setChecked(tab->ink_tool() == qt_pdftools::ui::PdfTabView::InkTool::kEraser);
}

QString MainWindow::BookmarkSidecarPath(const QString& pdf_path) const {
  const QString normalized = pdf_path.trimmed();
  if (normalized.isEmpty()) {
    return QString();
  }
  return normalized + QStringLiteral(".qtbookmarks.json");
}

QString MainWindow::InkSidecarPath(const QString& pdf_path) const {
  const QString normalized = pdf_path.trimmed();
  if (normalized.isEmpty()) {
    return QString();
  }
  return normalized + QStringLiteral(".qtink.json");
}

void MainWindow::LoadBookmarksForWidget(QWidget* widget, const QString& pdf_path) {
  if (widget == nullptr) {
    return;
  }

  bookmarks_by_tab_[widget].clear();
  widget->setProperty("bookmark_dirty", false);
  const QString sidecar_path = BookmarkSidecarPath(pdf_path);
  if (sidecar_path.isEmpty() || !QFileInfo::exists(sidecar_path)) {
    return;
  }

  QFile file(sidecar_path);
  if (!file.open(QIODevice::ReadOnly)) {
    AppendLog(QStringLiteral("读取书签 sidecar 失败: %1").arg(sidecar_path));
    return;
  }
  QJsonParseError parse_error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !doc.isArray()) {
    AppendLog(QStringLiteral("书签 sidecar 格式无效: %1").arg(sidecar_path));
    return;
  }

  QVariantList entries;
  for (const QJsonValue& value : doc.array()) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject obj = value.toObject();
    const int page = std::max(1, obj.value(QStringLiteral("page")).toInt(1));
    QString title = obj.value(QStringLiteral("title")).toString().trimmed();
    if (title.isEmpty()) {
      title = QStringLiteral("书签 P%1").arg(page);
    }
    QVariantMap entry;
    entry.insert(QStringLiteral("title"), title);
    entry.insert(QStringLiteral("page"), page);
    entries.push_back(entry);
  }
  bookmarks_by_tab_[widget] = entries;
}

bool MainWindow::SaveBookmarksForWidget(QWidget* widget, const QString& pdf_path) {
  if (widget == nullptr) {
    return true;
  }
  const QString sidecar_path = BookmarkSidecarPath(pdf_path);
  if (sidecar_path.isEmpty()) {
    return true;
  }

  const QVariantList entries = bookmarks_by_tab_.value(widget);
  if (entries.isEmpty()) {
    if (QFileInfo::exists(sidecar_path) && !QFile::remove(sidecar_path)) {
      AppendLog(QStringLiteral("清理书签 sidecar 失败: %1").arg(sidecar_path));
      return false;
    }
    widget->setProperty("bookmark_dirty", false);
    return true;
  }

  QJsonArray array;
  for (const QVariant& value : entries) {
    const QVariantMap entry = value.toMap();
    const int page = std::max(1, entry.value(QStringLiteral("page")).toInt());
    const QString title = entry.value(QStringLiteral("title")).toString().trimmed();
    QJsonObject obj;
    obj.insert(QStringLiteral("title"), title.isEmpty() ? QStringLiteral("书签 P%1").arg(page) : title);
    obj.insert(QStringLiteral("page"), page);
    array.push_back(obj);
  }

  QSaveFile file(sidecar_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    AppendLog(QStringLiteral("写入书签 sidecar 失败: %1").arg(sidecar_path));
    return false;
  }
  const QJsonDocument doc(array);
  if (file.write(doc.toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
    AppendLog(QStringLiteral("写入书签 sidecar 失败: %1").arg(sidecar_path));
    return false;
  }

  widget->setProperty("bookmark_dirty", false);
  return true;
}

bool MainWindow::LoadInkForWidget(QWidget* widget, const QString& pdf_path) {
  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr) {
    return true;
  }
  const QString sidecar_path = InkSidecarPath(pdf_path);
  if (sidecar_path.isEmpty()) {
    tab->ClearAllInk();
    return true;
  }

  QString error;
  if (!tab->LoadInkFromJson(sidecar_path, &error)) {
    AppendLog(QStringLiteral("读取墨迹 sidecar 失败: %1 (%2)").arg(sidecar_path, error));
    return false;
  }
  return true;
}

bool MainWindow::SaveInkForWidget(QWidget* widget, const QString& pdf_path) {
  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr) {
    return true;
  }
  const QString sidecar_path = InkSidecarPath(pdf_path);
  if (sidecar_path.isEmpty()) {
    return true;
  }

  QString error;
  if (!tab->SaveInkToJson(sidecar_path, &error)) {
    AppendLog(QStringLiteral("写入墨迹 sidecar 失败: %1 (%2)").arg(sidecar_path, error));
    return false;
  }
  return true;
}

int MainWindow::ReadPdfPageCount(const QString& pdf_path, QString* error_message) const {
  if (error_message != nullptr) {
    error_message->clear();
  }

  const QString normalized = QFileInfo(pdf_path.trimmed()).absoluteFilePath();
  if (normalized.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("PDF 路径为空");
    }
    return 0;
  }
  if (!QFileInfo::exists(normalized)) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("PDF 文件不存在: %1").arg(normalized);
    }
    return 0;
  }

  auto cached = pdf_page_count_cache_.constFind(normalized);
  if (cached != pdf_page_count_cache_.constEnd()) {
    return std::max(0, cached.value());
  }

#if QT_PDFTOOLS_HAS_QTPDF
  QPdfDocument doc;
  const QPdfDocument::Error err = doc.load(normalized);
  if (err != QPdfDocument::Error::None) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("读取页数失败: %1").arg(static_cast<int>(err));
    }
    return 0;
  }
  const int count = std::max(0, doc.pageCount());
  pdf_page_count_cache_[normalized] = count;
  return count;
#else
  if (error_message != nullptr) {
    *error_message = QStringLiteral("当前构建未启用 QtPdf，无法读取 PDF 页数");
  }
  return 0;
#endif
}

MainWindow::DocumentSession* MainWindow::EnsureSessionForTab(QWidget* widget) {
  if (widget == nullptr) {
    return nullptr;
  }
  auto it = sessions_by_tab_.find(widget);
  if (it != sessions_by_tab_.end()) {
    return &it.value();
  }

  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr) {
    return nullptr;
  }

  DocumentSession session;
  session.base_pdf_path = QFileInfo(tab->file_path().trimmed()).absoluteFilePath();
  const QString save_path = widget->property("save_path").toString().trimmed();
  session.save_path = save_path.isEmpty() ? session.base_pdf_path : QFileInfo(save_path).absoluteFilePath();
  session.dirty = widget->property("is_dirty").toBool();
  const int total_pages = std::max(0, tab->page_count());
  session.logical_pages.reserve(total_pages);
  for (int page = 1; page <= total_pages; ++page) {
    session.logical_pages.push_back(SessionPageRef{session.base_pdf_path, page});
  }

  sessions_by_tab_.insert(widget, std::move(session));
  return &sessions_by_tab_[widget];
}

const MainWindow::DocumentSession* MainWindow::SessionForTab(QWidget* widget) const {
  if (widget == nullptr) {
    return nullptr;
  }
  auto it = sessions_by_tab_.constFind(widget);
  if (it == sessions_by_tab_.constEnd()) {
    return nullptr;
  }
  return &it.value();
}

bool MainWindow::RebuildSessionLogicalPages(DocumentSession* session, QString* error_message) const {
  if (error_message != nullptr) {
    error_message->clear();
  }
  if (session == nullptr) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("session 为空");
    }
    return false;
  }

  QString page_error;
  const int base_pages = ReadPdfPageCount(session->base_pdf_path, &page_error);
  if (base_pages <= 0) {
    if (error_message != nullptr) {
      *error_message = page_error.isEmpty() ? QStringLiteral("基础 PDF 页数无效") : page_error;
    }
    return false;
  }

  QVector<SessionPageRef> pages;
  pages.reserve(base_pages);
  for (int page = 1; page <= base_pages; ++page) {
    pages.push_back(SessionPageRef{session->base_pdf_path, page});
  }

  for (const SessionOp& op : session->applied_ops) {
    if (op.type == SessionOpType::kMergeAppend || op.type == SessionOpType::kMergePrepend) {
      QString source_error;
      const int source_pages = ReadPdfPageCount(op.source_pdf, &source_error);
      if (source_pages <= 0) {
        if (error_message != nullptr) {
          *error_message = source_error.isEmpty() ? QStringLiteral("合并来源 PDF 页数无效") : source_error;
        }
        return false;
      }
      QVector<SessionPageRef> incoming;
      incoming.reserve(source_pages);
      const QString source_abs = QFileInfo(op.source_pdf).absoluteFilePath();
      for (int page = 1; page <= source_pages; ++page) {
        incoming.push_back(SessionPageRef{source_abs, page});
      }
      if (op.type == SessionOpType::kMergePrepend) {
        incoming += pages;
        pages = std::move(incoming);
      } else {
        pages += incoming;
      }
      continue;
    }

    if (op.type == SessionOpType::kDeletePage) {
      if (pages.size() <= 1) {
        if (error_message != nullptr) {
          *error_message = QStringLiteral("删除页会导致文档为空，已拒绝");
        }
        return false;
      }
      if (op.page < 1 || op.page > pages.size()) {
        if (error_message != nullptr) {
          *error_message = QStringLiteral("删除页码越界: %1 / %2").arg(op.page).arg(pages.size());
        }
        return false;
      }
      pages.removeAt(op.page - 1);
      continue;
    }

    if (op.type == SessionOpType::kSwapPages) {
      if (op.page < 1 || op.page > pages.size() || op.page_b < 1 || op.page_b > pages.size()) {
        if (error_message != nullptr) {
          *error_message =
              QStringLiteral("交换页码越界: A=%1 B=%2 total=%3").arg(op.page).arg(op.page_b).arg(pages.size());
        }
        return false;
      }
      if (op.page == op.page_b) {
        if (error_message != nullptr) {
          *error_message = QStringLiteral("交换页 A/B 不能相同");
        }
        return false;
      }
      pages.swapItemsAt(op.page - 1, op.page_b - 1);
      continue;
    }
  }

  session->logical_pages = std::move(pages);
  session->dirty = !session->applied_ops.isEmpty();
  return true;
}

QString MainWindow::DescribeSessionOp(const SessionOp& op) const {
  switch (op.type) {
    case SessionOpType::kMergeAppend:
      return QStringLiteral("合并（追加到末尾）");
    case SessionOpType::kMergePrepend:
      return QStringLiteral("合并（在开头插入）");
    case SessionOpType::kDeletePage:
      return QStringLiteral("删除页 %1").arg(op.page);
    case SessionOpType::kSwapPages:
      return QStringLiteral("交换页 %1 <-> %2").arg(op.page).arg(op.page_b);
  }
  return QStringLiteral("未知编辑操作");
}

bool MainWindow::ApplyEditOpToCurrentTab(const SessionOp& op) {
  if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
    AppendLog(QStringLiteral("当前没有可编辑的PDF标签页。"));
    return false;
  }

  QWidget* widget = preview_tabs_->currentWidget();
  auto* tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(widget);
  if (tab == nullptr) {
    AppendLog(QStringLiteral("当前标签页不是有效的PDF视图。"));
    return false;
  }

  DocumentSession* session = EnsureSessionForTab(widget);
  if (session == nullptr) {
    AppendLog(QStringLiteral("初始化会话失败，无法应用编辑。"));
    return false;
  }
  if (session->save_path.trimmed().isEmpty()) {
    QString save_path = widget->property("save_path").toString().trimmed();
    if (save_path.isEmpty()) {
      save_path = session->base_pdf_path;
    }
    session->save_path = QFileInfo(save_path).absoluteFilePath();
  }

  SessionOp normalized = op;
  if (normalized.type == SessionOpType::kMergeAppend || normalized.type == SessionOpType::kMergePrepend) {
    normalized.source_pdf = QFileInfo(normalized.source_pdf.trimmed()).absoluteFilePath();
    if (normalized.source_pdf.isEmpty() || !QFileInfo::exists(normalized.source_pdf)) {
      AppendLog(QStringLiteral("合并来源路径无效: %1").arg(op.source_pdf));
      return false;
    }
  }

  if (normalized.type == SessionOpType::kDeletePage && session->logical_pages.size() <= 1) {
    AppendLog(QStringLiteral("当前文档仅剩 1 页，无法继续删除。"));
    return false;
  }

  if (normalized.type == SessionOpType::kDeletePage) {
    const int total = session->logical_pages.size();
    if (normalized.page < 1 || normalized.page > total) {
      AppendLog(QStringLiteral("删除页码越界: %1 / %2").arg(normalized.page).arg(total));
      return false;
    }
  }

  if (normalized.type == SessionOpType::kSwapPages) {
    const int total = session->logical_pages.size();
    if (normalized.page < 1 || normalized.page > total || normalized.page_b < 1 || normalized.page_b > total) {
      AppendLog(QStringLiteral("交换页码越界: A=%1 B=%2 total=%3")
                    .arg(normalized.page)
                    .arg(normalized.page_b)
                    .arg(total));
      return false;
    }
    if (normalized.page == normalized.page_b) {
      AppendLog(QStringLiteral("交换页 A/B 不能相同。"));
      return false;
    }
  }

  session->applied_ops.push_back(normalized);
  session->redo_ops.clear();
  QString rebuild_error;
  if (!RebuildSessionLogicalPages(session, &rebuild_error)) {
    session->applied_ops.removeLast();
    AppendLog(QStringLiteral("应用编辑失败: %1").arg(rebuild_error));
    return false;
  }

  session->dirty = true;
  QString preview_error;
  if (!RenderSessionPreviewForTab(tab, &preview_error)) {
    session->applied_ops.removeLast();
    RebuildSessionLogicalPages(session, nullptr);
    AppendLog(QStringLiteral("应用编辑失败: 预览刷新失败: %1").arg(preview_error));
    return false;
  }
  const int index = preview_tabs_->currentIndex();
  SetTabDirty(index, true);
  UpdatePreviewFooter();
  UpdateUndoRedoActions();
  AppendLog(QStringLiteral("已应用编辑（会话内，保存时写入文件）: %1").arg(DescribeSessionOp(normalized)));
  statusBar()->showMessage(QStringLiteral("已应用编辑（未保存）"), 3000);
  return true;
}

bool MainWindow::BuildSessionOutputPdf(const DocumentSession& session,
                                       const QString& output_pdf,
                                       QString* error_message) {
  if (error_message != nullptr) {
    error_message->clear();
  }

  const QString input_base = QFileInfo(session.base_pdf_path).absoluteFilePath();
  const QString output_abs = QFileInfo(output_pdf).absoluteFilePath();
  if (input_base.isEmpty() || !QFileInfo::exists(input_base)) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("基础 PDF 不存在: %1").arg(input_base);
    }
    return false;
  }
  if (output_abs.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("输出路径为空");
    }
    return false;
  }

  backend::ITaskBackend* backend = backend_registry_.Get(active_backend_id_);
  if (backend == nullptr) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("backend 不存在");
    }
    return false;
  }
  const core::BackendStatus backend_status = backend->GetStatus();
  if (!backend_status.ready) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("backend 未就绪: %1").arg(backend_status.message);
    }
    return false;
  }

  QDir tmp_dir(QDir::tempPath() + QStringLiteral("/qt_pdftools_session"));
  if (!tmp_dir.exists() && !tmp_dir.mkpath(QStringLiteral("."))) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("无法创建 session 临时目录: %1").arg(tmp_dir.path());
    }
    return false;
  }

  QString current_input = input_base;
  const qint64 ts = QDateTime::currentMSecsSinceEpoch();
  QStringList generated_files;
  generated_files.reserve(session.applied_ops.size());

  auto cleanup_generated = [&generated_files]() {
    for (const QString& path : generated_files) {
      if (!path.isEmpty() && QFileInfo::exists(path)) {
        QFile::remove(path);
      }
    }
  };

  for (int i = 0; i < session.applied_ops.size(); ++i) {
    const SessionOp& op = session.applied_ops[i];
    const QString step_output = tmp_dir.filePath(QStringLiteral("session_%1_%2.pdf").arg(ts).arg(i + 1));

    core::TaskRequest request;
    request.output_pdf = step_output;
    if (op.type == SessionOpType::kMergeAppend || op.type == SessionOpType::kMergePrepend) {
      request.type = core::TaskType::kMerge;
      if (op.type == SessionOpType::kMergePrepend) {
        request.input_pdfs = QStringList() << op.source_pdf << current_input;
      } else {
        request.input_pdfs = QStringList() << current_input << op.source_pdf;
      }
    } else if (op.type == SessionOpType::kDeletePage) {
      request.type = core::TaskType::kDeletePage;
      request.input_pdf = current_input;
      request.page = op.page;
    } else if (op.type == SessionOpType::kSwapPages) {
      request.type = core::TaskType::kSwapPages;
      request.input_pdf = current_input;
      request.page = op.page;
      request.page_b = op.page_b;
    }

    const core::TaskResult result = backend->RunTask(request);
    if (!result.ok) {
      cleanup_generated();
      if (error_message != nullptr) {
        *error_message =
            QStringLiteral("%1: %2 (%3)").arg(DescribeSessionOp(op), result.error.message, result.error.code);
      }
      return false;
    }
    if (!QFileInfo::exists(step_output)) {
      cleanup_generated();
      if (error_message != nullptr) {
        *error_message = QStringLiteral("步骤输出不存在: %1").arg(step_output);
      }
      return false;
    }

    generated_files.push_back(step_output);
    current_input = step_output;
  }

  if (current_input != output_abs) {
    if (QFileInfo::exists(output_abs) && !QFile::remove(output_abs)) {
      cleanup_generated();
      if (error_message != nullptr) {
        *error_message = QStringLiteral("无法覆盖输出文件: %1").arg(output_abs);
      }
      return false;
    }
    if (!QFile::copy(current_input, output_abs)) {
      cleanup_generated();
      if (error_message != nullptr) {
        *error_message = QStringLiteral("写入输出文件失败: %1").arg(output_abs);
      }
      return false;
    }
  }

  cleanup_generated();
  return true;
}

bool MainWindow::RenderSessionPreviewForTab(qt_pdftools::ui::PdfTabView* tab, QString* error_message) {
  if (error_message != nullptr) {
    error_message->clear();
  }
  if (tab == nullptr) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("tab 为空");
    }
    return false;
  }

  QWidget* widget = qobject_cast<QWidget*>(tab);
  DocumentSession* session = EnsureSessionForTab(widget);
  if (session == nullptr) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("session 不存在");
    }
    return false;
  }

  const QString old_preview_path = tab->property("preview_temp_path").toString();

  if (session->applied_ops.isEmpty()) {
    const QString base_abs = QFileInfo(session->base_pdf_path).absoluteFilePath();
    if (base_abs.isEmpty() || !QFileInfo::exists(base_abs)) {
      if (error_message != nullptr) {
        *error_message = QStringLiteral("基础 PDF 不存在: %1").arg(base_abs);
      }
      return false;
    }

    QString load_error;
    if (tab->file_path() != base_abs && !tab->LoadFile(base_abs, &load_error)) {
      if (error_message != nullptr) {
        *error_message = QStringLiteral("恢复基础预览失败: %1").arg(load_error);
      }
      return false;
    }

    if (!old_preview_path.isEmpty() && old_preview_path != base_abs && QFileInfo::exists(old_preview_path) &&
        IsManagedTempPath(old_preview_path)) {
      QFile::remove(old_preview_path);
    }
    tab->setProperty("is_preview_temp", false);
    tab->setProperty("preview_temp_path", QString());
    tab->setProperty("save_path", session->save_path);
    return true;
  }

  QDir tmp_dir(QDir::tempPath() + QStringLiteral("/qt_pdftools_session"));
  if (!tmp_dir.exists() && !tmp_dir.mkpath(QStringLiteral("."))) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("无法创建预览目录: %1").arg(tmp_dir.path());
    }
    return false;
  }
  const qint64 ts = QDateTime::currentMSecsSinceEpoch();
  const QString preview_output =
      tmp_dir.filePath(QStringLiteral("preview_%1_%2.pdf").arg(ts).arg(reinterpret_cast<quintptr>(tab), 0, 16));

  QString build_error;
  if (!BuildSessionOutputPdf(*session, preview_output, &build_error)) {
    if (error_message != nullptr) {
      *error_message = build_error;
    }
    return false;
  }

  QString load_error;
  if (!tab->LoadFile(preview_output, &load_error)) {
    QFile::remove(preview_output);
    if (error_message != nullptr) {
      *error_message = QStringLiteral("加载预览失败: %1").arg(load_error);
    }
    return false;
  }

  tab->setProperty("is_preview_temp", true);
  tab->setProperty("preview_temp_path", preview_output);
  tab->setProperty("save_path", session->save_path);

  if (!old_preview_path.isEmpty() && old_preview_path != preview_output && QFileInfo::exists(old_preview_path) &&
      IsManagedTempPath(old_preview_path)) {
    QFile::remove(old_preview_path);
  }
  return true;
}

bool MainWindow::MaterializeSessionForTab(qt_pdftools::ui::PdfTabView* tab,
                                          const QString& target_pdf,
                                          QString* error_message) {
  if (error_message != nullptr) {
    error_message->clear();
  }
  if (tab == nullptr) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("tab 为空");
    }
    return false;
  }

  QWidget* widget = qobject_cast<QWidget*>(tab);
  DocumentSession* session = EnsureSessionForTab(widget);
  if (session == nullptr) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("session 不存在");
    }
    return false;
  }
  if (session->applied_ops.isEmpty()) {
    return true;
  }
  const QString target_abs = QFileInfo(target_pdf).absoluteFilePath();
  if (target_abs.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("目标保存路径为空");
    }
    return false;
  }
  const QString old_preview_path = tab->property("preview_temp_path").toString();
  QString build_error;
  if (!BuildSessionOutputPdf(*session, target_abs, &build_error)) {
    if (error_message != nullptr) {
      *error_message = build_error;
    }
    return false;
  }

  QString load_error;
  if (!tab->LoadFile(target_abs, &load_error)) {
    if (error_message != nullptr) {
      *error_message = QStringLiteral("保存后重载失败: %1").arg(load_error);
    }
    return false;
  }

  pdf_page_count_cache_.remove(target_abs);
  if (!old_preview_path.isEmpty() && old_preview_path != target_abs && QFileInfo::exists(old_preview_path) &&
      IsManagedTempPath(old_preview_path)) {
    QFile::remove(old_preview_path);
  }

  session->base_pdf_path = target_abs;
  session->save_path = target_abs;
  session->applied_ops.clear();
  session->redo_ops.clear();
  session->dirty = false;
  session->logical_pages.clear();
  const int total_pages = std::max(0, tab->page_count());
  session->logical_pages.reserve(total_pages);
  for (int page = 1; page <= total_pages; ++page) {
    session->logical_pages.push_back(SessionPageRef{target_abs, page});
  }

  tab->setProperty("is_preview_temp", false);
  tab->setProperty("preview_temp_path", QString());
  tab->setProperty("save_path", target_abs);
  return true;
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

void MainWindow::RunTaskForCurrentTab(const qt_pdftools::core::TaskRequest& request,
                                      bool preview_only,
                                      bool apply_current) {
  active_task_edit_current_ = false;
  active_task_target_tab_index_ = -1;

  if (apply_current) {
    if (preview_tabs_ == nullptr || preview_tabs_->currentIndex() < 0) {
      AppendLog(QStringLiteral("没有可应用编辑的目标标签页。"));
      return;
    }
    active_task_edit_current_ = true;
    active_task_target_tab_index_ = preview_tabs_->currentIndex();
  }

  RunTask(request, preview_only);
  if (!running_task_) {
    active_task_edit_current_ = false;
    active_task_target_tab_index_ = -1;
  }
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
        docx_run_button_,
        edit_current_merge_apply_button_,
        edit_current_delete_apply_button_,
        edit_current_swap_apply_button_,
        edit_current_extract_run_button_}) {
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
  const bool edit_current = active_task_edit_current_;
  const int target_tab_index = active_task_target_tab_index_;
  active_task_preview_ = false;
  active_task_edit_current_ = false;
  active_task_target_tab_index_ = -1;
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
        docx_run_button_,
        edit_current_merge_apply_button_,
        edit_current_delete_apply_button_,
        edit_current_swap_apply_button_,
        edit_current_extract_run_button_}) {
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
    const bool output_is_pdf =
        request.type == core::TaskType::kMerge || request.type == core::TaskType::kDeletePage ||
        request.type == core::TaskType::kInsertPage || request.type == core::TaskType::kReplacePage ||
        request.type == core::TaskType::kSwapPages;
    if (!result.output_path.isEmpty() && output_is_pdf) {
      if (edit_current && preview_tabs_ != nullptr && target_tab_index >= 0 &&
          target_tab_index < preview_tabs_->count()) {
        auto* target_tab = qobject_cast<qt_pdftools::ui::PdfTabView*>(preview_tabs_->widget(target_tab_index));
        if (target_tab != nullptr) {
          const QString previous_temp = target_tab->property("preview_temp_path").toString();
          QString save_path = target_tab->property("save_path").toString().trimmed();
          const QString old_file_path = target_tab->file_path();
          if (save_path.isEmpty()) {
            save_path = old_file_path;
          }

          QString load_error;
          if (target_tab->LoadFile(result.output_path, &load_error)) {
            target_tab->setProperty("is_preview_temp", true);
            target_tab->setProperty("preview_temp_path", result.output_path);
            target_tab->setProperty("save_path", save_path);
            preview_tabs_->setTabText(target_tab_index, Basename(save_path));
            preview_tabs_->setTabToolTip(target_tab_index, QStringLiteral("编辑中: %1").arg(save_path));
            sessions_by_tab_.remove(target_tab);
            if (DocumentSession* session = EnsureSessionForTab(target_tab)) {
              session->base_pdf_path = QFileInfo(result.output_path).absoluteFilePath();
              session->save_path = save_path;
              session->dirty = true;
              session->applied_ops.clear();
              session->redo_ops.clear();
              session->logical_pages.clear();
              const int total_pages = std::max(0, target_tab->page_count());
              session->logical_pages.reserve(total_pages);
              for (int page = 1; page <= total_pages; ++page) {
                session->logical_pages.push_back(SessionPageRef{session->base_pdf_path, page});
              }
            }
            undo_stack_by_tab_[target_tab].push_back(old_file_path);
            redo_stack_by_tab_[target_tab].clear();
            SetTabDirty(target_tab_index, true);
            preview_tabs_->setCurrentIndex(target_tab_index);
            RefreshTabSelectors();
            UpdatePreviewFooter();
            UpdateUndoRedoActions();

            const bool history_refs_previous =
                undo_stack_by_tab_[target_tab].contains(previous_temp) ||
                redo_stack_by_tab_[target_tab].contains(previous_temp);
            if (!previous_temp.isEmpty() && previous_temp != result.output_path &&
                QFileInfo::exists(previous_temp) && !history_refs_previous) {
              QFile::remove(previous_temp);
            }

            AppendLog(QStringLiteral("已应用到当前标签页（未保存）"));
            statusBar()->showMessage(QStringLiteral("已应用到当前标签页"), 4000);
            return;
          }

          AppendLog(QStringLiteral("应用到当前标签页失败，已回退为新标签页打开: %1").arg(load_error));
        }
      }

      if (preview_only) {
        const QString preview_title =
            QStringLiteral("[预览] %1 #%2").arg(core::TaskTypeToString(request.type)).arg(task_id);
        OpenPdfTab(result.output_path, true, preview_title, true);
        if (preview_tabs_ != nullptr && preview_tabs_->currentIndex() >= 0) {
          const QString summary = BuildTaskInputSummary(request);
          preview_tabs_->setTabToolTip(
              preview_tabs_->currentIndex(),
              QStringLiteral("[预览] %1\n%2").arg(result.output_path, summary));
        }
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

QString MainWindow::BuildTaskInputSummary(const qt_pdftools::core::TaskRequest& request) const {
  switch (request.type) {
    case core::TaskType::kMerge:
      return QStringLiteral("inputs=%1").arg(request.input_pdfs.join(QStringLiteral(", ")));
    case core::TaskType::kDeletePage:
      return QStringLiteral("input=%1, page=%2").arg(request.input_pdf).arg(request.page);
    case core::TaskType::kInsertPage:
      return QStringLiteral("target=%1, source=%2, at=%3, sourcePage=%4")
          .arg(request.input_pdf)
          .arg(request.source_pdf)
          .arg(request.at)
          .arg(request.source_page);
    case core::TaskType::kReplacePage:
      return QStringLiteral("target=%1, source=%2, page=%3, sourcePage=%4")
          .arg(request.input_pdf)
          .arg(request.source_pdf)
          .arg(request.page)
          .arg(request.source_page);
    case core::TaskType::kSwapPages:
      return QStringLiteral("input=%1, pageA=%2, pageB=%3")
          .arg(request.input_pdf)
          .arg(request.page)
          .arg(request.page_b);
    case core::TaskType::kExtractImages:
      return QStringLiteral("input=%1, outDir=%2, pageStart=%3, pageEnd=%4")
          .arg(request.input_pdf)
          .arg(request.output_dir)
          .arg(request.page)
          .arg(request.page_b);
    case core::TaskType::kPdfToDocx:
      return QStringLiteral("input=%1, output=%2, noImages=%3, anchored=%4")
          .arg(request.input_pdf)
          .arg(request.output_docx)
          .arg(request.no_images ? QStringLiteral("true") : QStringLiteral("false"))
          .arg(request.anchored_images ? QStringLiteral("true") : QStringLiteral("false"));
  }
  return QString();
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
    case core::TaskType::kSwapPages:
      op_name = QStringLiteral("swap");
      break;
    case core::TaskType::kExtractImages:
      op_name = QStringLiteral("extract-images");
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
