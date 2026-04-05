#pragma once

#include <memory>

#include <QHash>
#include <QMainWindow>
#include <QPointer>

#include "qt_pdftools/backend/backend_registry.hpp"
#include "qt_pdftools/core/app_settings.hpp"
#include "qt_pdftools/core/task_types.hpp"

class QCheckBox;
class QComboBox;
class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
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
  QWidget* BuildEditCurrentPage();
  QWidget* BuildExtractImagesPage();
  QWidget* BuildAnnotationBookmarkPage();
  QWidget* BuildPreviewArea();

  void BindSignals();
  void BindMergeSignals();
  void BindDeleteSignals();
  void BindInsertSignals();
  void BindReplaceSignals();
  void BindDocxSignals();
  void BindEditCurrentSignals();
  void BindExtractImagesSignals();
  void BindAnnotationBookmarkSignals();

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
  bool UndoCurrentTab();
  bool RedoCurrentTab();
  void UpdateUndoRedoActions();
  bool IsManagedTempPath(const QString& path) const;
  void CloseCurrentPdfTab();
  void CloseAllPdfTabs();
  void CleanupPreviewTempForWidget(QWidget* widget, bool write_log = true);

  qt_pdftools::ui::PdfTabView* ActivePdfTab() const;
  QString ActivePdfPath() const;

  void UpdatePreviewFooter();
  void RefreshTabSelectors();
  void RefreshBookmarkPanel();

  void RunTask(const qt_pdftools::core::TaskRequest& request, bool preview_only = false);
  void RunTaskForCurrentTab(const qt_pdftools::core::TaskRequest& request, bool preview_only, bool apply_current);
  void StartTaskExecution(const qt_pdftools::core::TaskRequest& request, bool preview_only);
  void HandleTaskFinished(const qt_pdftools::core::TaskRequest& request,
                          const qt_pdftools::core::TaskResult& result);

  QString DialogInitialPath(const QString& preferred = QString()) const;
  void RememberDialogPath(const QString& selected_path);
  QString BuildPreviewOutputPath(qt_pdftools::core::TaskType type) const;
  QString BuildTaskInputSummary(const qt_pdftools::core::TaskRequest& request) const;
  QString BookmarkSidecarPath(const QString& pdf_path) const;
  QString InkSidecarPath(const QString& pdf_path) const;
  void LoadBookmarksForWidget(QWidget* widget, const QString& pdf_path);
  bool SaveBookmarksForWidget(QWidget* widget, const QString& pdf_path);
  bool LoadInkForWidget(QWidget* widget, const QString& pdf_path);
  bool SaveInkForWidget(QWidget* widget, const QString& pdf_path);

  QStringList ParseMultilinePaths(const QString& raw_text) const;
  QString SuggestOutput(const QString& input_path, const QString& suffix, const QString& extension) const;

 private:
  std::unique_ptr<QSettings> settings_;
  qt_pdftools::core::AppSettings app_settings_;
  QString active_backend_id_ = QStringLiteral("native-lib");

  qt_pdftools::backend::BackendRegistry backend_registry_;

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

  QLineEdit* edit_current_merge_source_edit_ = nullptr;
  QRadioButton* edit_current_merge_append_radio_ = nullptr;
  QRadioButton* edit_current_merge_prepend_radio_ = nullptr;
  QPushButton* edit_current_merge_apply_button_ = nullptr;
  QSpinBox* edit_current_delete_page_spin_ = nullptr;
  QPushButton* edit_current_delete_apply_button_ = nullptr;
  QSpinBox* edit_current_swap_page_a_spin_ = nullptr;
  QSpinBox* edit_current_swap_page_b_spin_ = nullptr;
  QPushButton* edit_current_swap_apply_button_ = nullptr;
  QSpinBox* edit_current_extract_from_spin_ = nullptr;
  QSpinBox* edit_current_extract_to_spin_ = nullptr;
  QLineEdit* edit_current_extract_output_dir_edit_ = nullptr;
  QPushButton* edit_current_extract_run_button_ = nullptr;

  QListWidget* bookmark_list_ = nullptr;
  QPushButton* bookmark_add_button_ = nullptr;
  QPushButton* bookmark_jump_button_ = nullptr;
  QPushButton* bookmark_rename_button_ = nullptr;
  QPushButton* bookmark_delete_button_ = nullptr;
  QPushButton* annotation_pen_button_ = nullptr;
  QPushButton* annotation_marker_button_ = nullptr;
  QPushButton* annotation_eraser_button_ = nullptr;
  QPushButton* annotation_undo_button_ = nullptr;
  QPushButton* annotation_clear_button_ = nullptr;

  bool running_task_ = false;
  bool active_task_preview_ = false;
  bool active_task_edit_current_ = false;
  int active_task_target_tab_index_ = -1;
  QPointer<QFutureWatcher<qt_pdftools::core::TaskResult>> task_watcher_;
  QPointer<QAction> undo_action_;
  QPointer<QAction> redo_action_;
  QHash<QWidget*, QStringList> undo_stack_by_tab_;
  QHash<QWidget*, QStringList> redo_stack_by_tab_;
  QHash<QWidget*, QVariantList> bookmarks_by_tab_;
  int next_task_id_ = 1;
  int active_task_id_ = 0;
};

}  // namespace qt_pdftools::app
