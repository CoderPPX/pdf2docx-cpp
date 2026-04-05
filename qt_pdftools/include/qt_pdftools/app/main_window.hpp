#pragma once

#include <memory>

#include <QMainWindow>
#include <QPointer>

#include "qt_pdftools/backend/backend_registry.hpp"
#include "qt_pdftools/core/app_settings.hpp"
#include "qt_pdftools/core/task_types.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSettings;
class QSpinBox;
class QTabWidget;
class QTextEdit;
template <typename T>
class QFutureWatcher;
class QCloseEvent;

namespace qt_pdftools::ui {
class PdfTabView;
}

namespace qt_pdftools::app {

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void BuildUi();
  void BuildMenu();
  QWidget* BuildSidebar();
  QWidget* BuildMergePage();
  QWidget* BuildDeletePage();
  QWidget* BuildInsertPage();
  QWidget* BuildReplacePage();
  QWidget* BuildDocxPage();
  QWidget* BuildPreviewArea();

  void BindSignals();
  void BindMergeSignals();
  void BindDeleteSignals();
  void BindInsertSignals();
  void BindReplaceSignals();
  void BindDocxSignals();

  void AppendLog(const QString& message);
  void ApplyTheme(const QString& theme_mode);

  void OpenSettingsDialog();
  void RefreshBackendStatus(bool write_log = true);
  bool IsBackendReady();

  void OpenPdfTabs(const QStringList& paths, bool focus_last = true);
  void OpenPdfTab(const QString& path,
                  bool focus = true,
                  const QString& custom_title = QString(),
                  bool is_preview_temp = false);
  bool SaveTabByIndex(int index, bool force_save_as = false);
  bool ConfirmCloseTab(int index);
  bool RequestCloseTab(int index);
  void SetTabDirty(int index, bool dirty);
  bool IsTabDirty(int index) const;
  void EnsureTabCloseButton(int index);
  void RefreshTabCloseButton(int index);
  void CloseCurrentPdfTab();
  void CloseAllPdfTabs();
  void CleanupPreviewTempForWidget(QWidget* widget, bool write_log = true);

  qt_pdftools::ui::PdfTabView* ActivePdfTab() const;
  QString ActivePdfPath() const;

  void UpdatePreviewFooter();
  void RefreshTabSelectors();

  void RunTask(const qt_pdftools::core::TaskRequest& request, bool preview_only = false);
  void StartTaskExecution(const qt_pdftools::core::TaskRequest& request, bool preview_only);
  void HandleTaskFinished(const qt_pdftools::core::TaskRequest& request,
                          const qt_pdftools::core::TaskResult& result);

  QString DialogInitialPath(const QString& preferred = QString()) const;
  void RememberDialogPath(const QString& selected_path);
  QString BuildPreviewOutputPath(qt_pdftools::core::TaskType type) const;

  QStringList ParseMultilinePaths(const QString& raw_text) const;
  QString SuggestOutput(const QString& input_path, const QString& suffix, const QString& extension) const;

 private:
  std::unique_ptr<QSettings> settings_;
  qt_pdftools::core::AppSettings app_settings_;
  QString active_backend_id_ = QStringLiteral("native-lib");

  qt_pdftools::backend::BackendRegistry backend_registry_;

  QPointer<QTabWidget> tool_tabs_;
  QPointer<QTabWidget> preview_tabs_;

  QPointer<QTextEdit> task_log_;

  QPointer<QLabel> preview_current_label_;
  QPointer<QLabel> preview_page_label_;
  QPointer<QLabel> preview_zoom_label_;

  QPointer<QPushButton> preview_prev_button_;
  QPointer<QPushButton> preview_next_button_;
  QPointer<QPushButton> preview_zoom_in_button_;
  QPointer<QPushButton> preview_zoom_out_button_;

  QListWidget* merge_tab_select_ = nullptr;
  QTextEdit* merge_inputs_edit_ = nullptr;
  QLineEdit* merge_output_edit_ = nullptr;
  QPushButton* merge_run_button_ = nullptr;
  QPushButton* merge_preview_button_ = nullptr;

  QComboBox* delete_input_tab_combo_ = nullptr;
  QLineEdit* delete_input_edit_ = nullptr;
  QLineEdit* delete_output_edit_ = nullptr;
  QSpinBox* delete_page_spin_ = nullptr;
  QPushButton* delete_run_button_ = nullptr;
  QPushButton* delete_preview_button_ = nullptr;

  QComboBox* insert_target_tab_combo_ = nullptr;
  QLineEdit* insert_target_edit_ = nullptr;
  QComboBox* insert_source_tab_combo_ = nullptr;
  QLineEdit* insert_source_edit_ = nullptr;
  QLineEdit* insert_output_edit_ = nullptr;
  QSpinBox* insert_at_spin_ = nullptr;
  QSpinBox* insert_source_page_spin_ = nullptr;
  QPushButton* insert_run_button_ = nullptr;
  QPushButton* insert_preview_button_ = nullptr;

  QComboBox* replace_target_tab_combo_ = nullptr;
  QLineEdit* replace_target_edit_ = nullptr;
  QComboBox* replace_source_tab_combo_ = nullptr;
  QLineEdit* replace_source_edit_ = nullptr;
  QLineEdit* replace_output_edit_ = nullptr;
  QSpinBox* replace_page_spin_ = nullptr;
  QSpinBox* replace_source_page_spin_ = nullptr;
  QPushButton* replace_run_button_ = nullptr;
  QPushButton* replace_preview_button_ = nullptr;

  QComboBox* docx_input_tab_combo_ = nullptr;
  QLineEdit* docx_input_edit_ = nullptr;
  QLineEdit* docx_output_edit_ = nullptr;
  QLineEdit* docx_dump_ir_edit_ = nullptr;
  QCheckBox* docx_no_images_check_ = nullptr;
  QCheckBox* docx_anchored_check_ = nullptr;
  QPushButton* docx_run_button_ = nullptr;

  bool running_task_ = false;
  bool active_task_preview_ = false;
  QPointer<QFutureWatcher<qt_pdftools::core::TaskResult>> task_watcher_;
  int next_task_id_ = 1;
  int active_task_id_ = 0;
};

}  // namespace qt_pdftools::app
