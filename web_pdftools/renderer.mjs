import * as pdfjsLib from './node_modules/pdfjs-dist/legacy/build/pdf.mjs';
import {
  BackendId,
  WasmBackendMode,
  createTaskBackendRegistry,
  resolveBackendId
} from './backends/task_backends.mjs';

pdfjsLib.GlobalWorkerOptions.workerSrc = new URL(
  './node_modules/pdfjs-dist/legacy/build/pdf.worker.mjs',
  import.meta.url
).toString();

const api = window.pdftoolsAPI;

const logEl = document.getElementById('task-log');
const statusEl = document.getElementById('binary-status');
const previewCurrentEl = document.getElementById('preview-current');
const tabsEl = document.getElementById('pdf-tabs');
const previewEmptyEl = document.getElementById('preview-empty');
const previewCanvasWrapEl = document.getElementById('preview-canvas-wrap');
const previewCanvasEl = document.getElementById('pdf-canvas');
const previewPageLabelEl = document.getElementById('preview-page-label');
const previewZoomLabelEl = document.getElementById('preview-zoom-label');
const settingsModalEl = document.getElementById('settings-modal');
const settingsThemeModeEl = document.getElementById('settings-theme-mode');
const settingsBackendModeEl = document.getElementById('settings-backend-mode');
const settingsWasmFallbackEl = document.getElementById('settings-wasm-fallback');
const settingsSaveEl = document.getElementById('settings-save');
const settingsCancelEl = document.getElementById('settings-cancel');
const settingsCloseEl = document.getElementById('settings-close');
const bridgeStatStatusEl = document.getElementById('bridge-stat-status');
const bridgeStatTaskEl = document.getElementById('bridge-stat-task');
const bridgeStatStageEl = document.getElementById('bridge-stat-stage');
const bridgeStatTimeEl = document.getElementById('bridge-stat-time');
const bridgeStatFilesEl = document.getElementById('bridge-stat-files');
const bridgeStatBytesEl = document.getElementById('bridge-stat-bytes');
const bridgeStatUpdatedEl = document.getElementById('bridge-stat-updated');
const bridgeStatAvgEl = document.getElementById('bridge-stat-avg');
const bridgeStatSuccessEl = document.getElementById('bridge-stat-success');
const bridgeStatFallbackEl = document.getElementById('bridge-stat-fallback');
const bridgeStatFallbackTimeEl = document.getElementById('bridge-stat-fallback-time');
const bridgeStatsResetEl = document.getElementById('bridge-stats-reset');

const themeStorageKey = 'web_pdftools.theme.mode';
const backendStorageKey = 'web_pdftools.backend.mode';
const wasmFallbackStorageKey = 'web_pdftools.wasm.autoFallback';
const themeModes = new Set(['dark', 'light', 'system']);
const systemDarkMediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
const wasmBridgeTaskTypes = new Set(['merge', 'delete-page', 'insert-page', 'replace-page', 'pdf2docx']);
const wasmFallbackCodes = new Set([
  'UNSUPPORTED_FEATURE',
  'BACKEND_RESERVED',
  'WASM_RUN_FAILED',
  'WASM_OP_FAILED',
  'WASM_API_MISSING',
  'WORKER_RUNTIME_ERROR',
  'WORKER_CLOSED',
  'WORKER_UNAVAILABLE',
  'NOT_INITIALIZED',
  'IO_ERROR'
]);
const wasmPreviewHintCodes = new Set(['UNSUPPORTED_FEATURE', 'BACKEND_RESERVED', 'NOT_IMPLEMENTED']);

const tabSelectors = [
  'merge-tab-select',
  'delete-input-tab',
  'insert-target-tab',
  'insert-source-tab',
  'replace-target-tab',
  'replace-source-tab',
  'docx-input-tab'
];

const singleTabSelectBindings = [
  ['delete-input-tab', 'delete-input'],
  ['insert-target-tab', 'insert-target'],
  ['insert-source-tab', 'insert-source'],
  ['replace-target-tab', 'replace-target'],
  ['replace-source-tab', 'replace-source'],
  ['docx-input-tab', 'docx-input']
];

const previewTabs = [];
const bridgeHistoryTotals = [];
const bridgeHistoryOutcomes = [];
let activeTabId = null;
let running = false;
let renderToken = 0;
let activeThemeMode = 'dark';
let wasmAutoFallbackEnabled = true;
let lastFallbackAt = '--';
const backendRegistry = createTaskBackendRegistry(api, {
  wasmMode: WasmBackendMode.Worker
});
let activeBackendId = BackendId.NativeCli;

function now() {
  return new Date().toLocaleTimeString();
}

function formatBytes(bytes) {
  const value = Number(bytes);
  if (!Number.isFinite(value) || value < 0) {
    return '--';
  }
  if (value < 1024) {
    return `${Math.round(value)} B`;
  }
  if (value < 1024 * 1024) {
    return `${(value / 1024).toFixed(1)} KB`;
  }
  return `${(value / (1024 * 1024)).toFixed(2)} MB`;
}

function updateBridgeStatsView(update = {}) {
  if (!bridgeStatStatusEl) {
    return;
  }

  const status = update.status || 'pending';
  bridgeStatStatusEl.className = `status ${status}`;
  if (update.statusText) {
    bridgeStatStatusEl.textContent = update.statusText;
  }
  if (bridgeStatTaskEl && update.task) {
    bridgeStatTaskEl.textContent = update.task;
  }
  if (bridgeStatStageEl && update.stage) {
    bridgeStatStageEl.textContent = update.stage;
  }
  if (bridgeStatTimeEl && update.time) {
    bridgeStatTimeEl.textContent = update.time;
  }
  if (bridgeStatFilesEl && update.files) {
    bridgeStatFilesEl.textContent = update.files;
  }
  if (bridgeStatBytesEl && update.bytes) {
    bridgeStatBytesEl.textContent = update.bytes;
  }
  if (bridgeStatAvgEl && update.avg) {
    bridgeStatAvgEl.textContent = update.avg;
  }
  if (bridgeStatSuccessEl && update.success) {
    bridgeStatSuccessEl.textContent = update.success;
  }
  if (bridgeStatFallbackEl && update.fallback) {
    bridgeStatFallbackEl.textContent = update.fallback;
  }
  if (bridgeStatFallbackTimeEl && update.fallbackTime) {
    bridgeStatFallbackTimeEl.textContent = update.fallbackTime;
  }
  if (bridgeStatUpdatedEl) {
    bridgeStatUpdatedEl.textContent = now();
  }
}

function recordBridgeTotal(totalMs) {
  const value = Number(totalMs);
  if (!Number.isFinite(value) || value < 0) {
    return;
  }
  bridgeHistoryTotals.push(value);
  if (bridgeHistoryTotals.length > 5) {
    bridgeHistoryTotals.shift();
  }
}

function formatBridgeAverage() {
  if (bridgeHistoryTotals.length === 0) {
    return '--';
  }
  const sum = bridgeHistoryTotals.reduce((acc, value) => acc + value, 0);
  const avg = sum / bridgeHistoryTotals.length;
  return `${avg.toFixed(1)} ms`;
}

function recordBridgeOutcome(ok) {
  bridgeHistoryOutcomes.push(Boolean(ok));
  if (bridgeHistoryOutcomes.length > 5) {
    bridgeHistoryOutcomes.shift();
  }
}

function formatBridgeSuccessRate() {
  if (bridgeHistoryOutcomes.length === 0) {
    return '--';
  }
  const successCount = bridgeHistoryOutcomes.filter(Boolean).length;
  const total = bridgeHistoryOutcomes.length;
  const ratio = (successCount / total) * 100;
  return `${successCount}/${total} (${ratio.toFixed(0)}%)`;
}

function buildDefaultBridgeStatsState() {
  return {
    status: 'pending',
    statusText: '尚无任务',
    task: '--',
    stage: '--',
    time: '--',
    files: '--',
    bytes: '--',
    avg: formatBridgeAverage(),
    success: formatBridgeSuccessRate(),
    fallback: '--',
    fallbackTime: lastFallbackAt
  };
}

function resetBridgeStats({ log = true } = {}) {
  lastFallbackAt = '--';
  bridgeHistoryTotals.length = 0;
  bridgeHistoryOutcomes.length = 0;
  updateBridgeStatsView(buildDefaultBridgeStatsState());
  if (log) {
    appendLog('WASM Bridge 统计已清空。');
  }
}

function shouldFallbackFromWasm(result) {
  if (!result || result.ok) {
    return false;
  }
  return wasmFallbackCodes.has(result.code);
}

function appendLog(message) {
  const line = `[${now()}] ${message}`;
  if (logEl.textContent) {
    logEl.textContent += `\n${line}`;
  } else {
    logEl.textContent = line;
  }
  logEl.scrollTop = logEl.scrollHeight;
}

function getBackendById(id) {
  return backendRegistry.get(id) || null;
}

function getActiveBackend() {
  return getBackendById(activeBackendId) || getBackendById(BackendId.NativeCli);
}

function normalizeBackendId(id) {
  const resolved = resolveBackendId(id, BackendId.NativeCli);
  return backendRegistry.has(resolved) ? resolved : BackendId.NativeCli;
}

async function refreshBackendStatus({ log = true } = {}) {
  const backend = getActiveBackend();
  if (!backend) {
    statusEl.className = 'status error';
    statusEl.textContent = '后端不可用';
    if (log) {
      appendLog('未找到可用 backend。');
    }
    return;
  }

  try {
    const status = await backend.getStatus();
    if (backend.id === BackendId.Wasm && status.ready) {
      statusEl.className = 'status ok';
      statusEl.textContent = status.message;
    } else if (backend.id === BackendId.Wasm) {
      statusEl.className = 'status pending';
      statusEl.textContent = status.message;
    } else if (status.ready) {
      statusEl.className = 'status ok';
      statusEl.textContent = status.message;
    } else {
      statusEl.className = 'status error';
      statusEl.textContent = status.message;
    }

    if (log) {
      appendLog(`当前 backend: ${backend.label}`);
      appendLog(`backend 状态: ${status.message}`);
    }
  } catch (error) {
    statusEl.className = 'status error';
    statusEl.textContent = `状态检测失败: ${backend.label}`;
    if (log) {
      appendLog(`backend 状态检测异常: ${error.message || String(error)}`);
    }
  }
}

function applyBackendMode(mode, { persist = true, log = true, refreshStatus = true } = {}) {
  const normalized = normalizeBackendId(mode);
  activeBackendId = normalized;

  if (settingsBackendModeEl) {
    settingsBackendModeEl.value = normalized;
  }

  if (persist) {
    try {
      window.localStorage.setItem(backendStorageKey, normalized);
    } catch (_error) {
      // no-op
    }
  }

  const backend = getActiveBackend();
  if (log && backend) {
    appendLog(`backend 切换: ${backend.label}`);
  }

  if (refreshStatus) {
    void refreshBackendStatus({ log: false });
  }
}

function normalizeBooleanStorage(value, fallback = true) {
  if (typeof value === 'boolean') {
    return value;
  }
  if (value === 'true' || value === '1') {
    return true;
  }
  if (value === 'false' || value === '0') {
    return false;
  }
  return fallback;
}

function applyWasmFallbackSetting(enabled, { persist = true, log = true } = {}) {
  const normalized = Boolean(enabled);
  wasmAutoFallbackEnabled = normalized;

  if (settingsWasmFallbackEl) {
    settingsWasmFallbackEl.checked = normalized;
  }

  if (persist) {
    try {
      window.localStorage.setItem(wasmFallbackStorageKey, normalized ? 'true' : 'false');
    } catch (_error) {
      // no-op
    }
  }

  if (log) {
    appendLog(`WASM 自动回退: ${normalized ? '开启' : '关闭'}`);
  }
}

function normalizeThemeMode(mode) {
  return themeModes.has(mode) ? mode : 'dark';
}

function resolveTheme(mode) {
  if (mode === 'system') {
    return systemDarkMediaQuery.matches ? 'dark' : 'light';
  }
  return mode;
}

function applyThemeMode(mode, { persist = true, log = true } = {}) {
  const normalizedMode = normalizeThemeMode(mode);
  const resolvedTheme = resolveTheme(normalizedMode);
  activeThemeMode = normalizedMode;

  document.documentElement.setAttribute('data-theme-mode', normalizedMode);
  document.documentElement.setAttribute('data-theme', resolvedTheme);

  if (settingsThemeModeEl) {
    settingsThemeModeEl.value = normalizedMode;
  }

  if (persist) {
    try {
      window.localStorage.setItem(themeStorageKey, normalizedMode);
    } catch (_error) {
      // no-op
    }
  }

  if (log) {
    appendLog(`主题切换: ${normalizedMode} (${resolvedTheme})`);
  }
}

function openThemeSettings() {
  if (!settingsModalEl) {
    appendLog('Theme Settings UI 未找到。');
    return;
  }

  settingsThemeModeEl.value = activeThemeMode;
  if (settingsBackendModeEl) {
    settingsBackendModeEl.value = activeBackendId;
  }
  if (settingsWasmFallbackEl) {
    settingsWasmFallbackEl.checked = wasmAutoFallbackEnabled;
  }
  settingsModalEl.classList.add('open');
  settingsModalEl.setAttribute('aria-hidden', 'false');
  document.body.classList.add('modal-open');
}

function closeThemeSettings() {
  if (!settingsModalEl) {
    return;
  }
  settingsModalEl.classList.remove('open');
  settingsModalEl.setAttribute('aria-hidden', 'true');
  document.body.classList.remove('modal-open');
}

function initThemeSettings() {
  let initialMode = 'dark';
  try {
    initialMode = normalizeThemeMode(window.localStorage.getItem(themeStorageKey));
  } catch (_error) {
    // no-op
  }

  applyThemeMode(initialMode, { persist: false, log: false });

  if (typeof systemDarkMediaQuery.addEventListener === 'function') {
    systemDarkMediaQuery.addEventListener('change', () => {
      if (activeThemeMode === 'system') {
        applyThemeMode('system', { persist: false, log: true });
      }
    });
  } else if (typeof systemDarkMediaQuery.addListener === 'function') {
    systemDarkMediaQuery.addListener(() => {
      if (activeThemeMode === 'system') {
        applyThemeMode('system', { persist: false, log: true });
      }
    });
  }
}

function initBackendSettings() {
  let initialMode = BackendId.NativeCli;
  try {
    initialMode = normalizeBackendId(window.localStorage.getItem(backendStorageKey));
  } catch (_error) {
    // no-op
  }

  applyBackendMode(initialMode, { persist: false, log: false, refreshStatus: false });
}

function initWasmFallbackSettings() {
  let initialEnabled = true;
  try {
    initialEnabled = normalizeBooleanStorage(window.localStorage.getItem(wasmFallbackStorageKey), true);
  } catch (_error) {
    // no-op
  }
  applyWasmFallbackSetting(initialEnabled, { persist: false, log: false });
}

function bindThemeSettings() {
  if (
    !settingsModalEl ||
    !settingsThemeModeEl ||
    !settingsBackendModeEl ||
    !settingsWasmFallbackEl ||
    !settingsSaveEl ||
    !settingsCancelEl ||
    !settingsCloseEl
  ) {
    return;
  }

  settingsSaveEl.addEventListener('click', async () => {
    applyThemeMode(settingsThemeModeEl.value, { persist: true, log: true });
    applyBackendMode(settingsBackendModeEl.value, { persist: true, log: true, refreshStatus: false });
    applyWasmFallbackSetting(settingsWasmFallbackEl.checked, { persist: true, log: true });
    await refreshBackendStatus({ log: true });
    closeThemeSettings();
  });

  settingsCancelEl.addEventListener('click', () => {
    closeThemeSettings();
  });

  settingsCloseEl.addEventListener('click', () => {
    closeThemeSettings();
  });

  settingsModalEl.addEventListener('click', (event) => {
    if (event.target === settingsModalEl) {
      closeThemeSettings();
    }
  });

  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && settingsModalEl.classList.contains('open')) {
      closeThemeSettings();
    }
  });
}

function bindBridgeStatsControls() {
  if (!bridgeStatsResetEl) {
    return;
  }

  bridgeStatsResetEl.addEventListener('click', () => {
    resetBridgeStats({ log: true });
  });
}

function setButtonsDisabled(disabled) {
  running = disabled;
  document.querySelectorAll('button.primary').forEach((button) => {
    button.disabled = disabled;
  });
}

function ensureIdle() {
  if (!running) {
    return true;
  }
  appendLog('已有任务执行中，请等待当前任务结束。');
  return false;
}

function parseMultiLinePaths(value) {
  return value
    .split(/\r?\n/)
    .map((item) => item.trim())
    .filter(Boolean);
}

function basename(filePath) {
  const normalized = filePath.replace(/\\/g, '/');
  const parts = normalized.split('/').filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : filePath;
}

function showPreviewPlaceholder(title, description) {
  const titleEl = previewEmptyEl.querySelector('h3');
  const textEl = previewEmptyEl.querySelector('p');
  titleEl.textContent = title;
  textEl.textContent = description;
  previewEmptyEl.style.display = 'grid';
  previewCanvasWrapEl.classList.remove('active');
}

function showPreviewCanvas() {
  previewEmptyEl.style.display = 'none';
  previewCanvasWrapEl.classList.add('active');
}

function getActiveTab() {
  return previewTabs.find((tab) => tab.id === activeTabId) || null;
}

function refreshPreviewLabels(tab) {
  if (!tab) {
    previewCurrentEl.textContent = '当前未打开 PDF';
    previewPageLabelEl.textContent = '0 / 0';
    previewZoomLabelEl.textContent = '100%';
    return;
  }

  previewCurrentEl.textContent = tab.path;
  const totalPages = tab.totalPages || 0;
  const currentPage = totalPages === 0 ? 0 : tab.currentPage;
  previewPageLabelEl.textContent = `${currentPage} / ${totalPages}`;
  previewZoomLabelEl.textContent = `${Math.round(tab.scale * 100)}%`;
}

function renderTabs() {
  tabsEl.innerHTML = '';
  tabsEl.classList.toggle('empty', previewTabs.length === 0);

  for (const tab of previewTabs) {
    const item = document.createElement('div');
    item.className = `tab-item${tab.id === activeTabId ? ' active' : ''}`;

    const labelBtn = document.createElement('button');
    labelBtn.type = 'button';
    labelBtn.className = 'tab-label';
    labelBtn.title = tab.path;
    labelBtn.textContent = tab.name;
    labelBtn.addEventListener('click', () => {
      setActiveTab(tab.id);
    });

    const closeBtn = document.createElement('button');
    closeBtn.type = 'button';
    closeBtn.className = 'tab-close';
    closeBtn.title = `关闭 ${tab.name}`;
    closeBtn.textContent = '×';
    closeBtn.addEventListener('click', (event) => {
      event.stopPropagation();
      closeTab(tab.id);
    });

    item.appendChild(labelBtn);
    item.appendChild(closeBtn);
    tabsEl.appendChild(item);
  }
}

function refreshTabSelectors() {
  for (const id of tabSelectors) {
    const select = document.getElementById(id);
    if (!select) {
      continue;
    }

    const selectedValues = new Set(
      Array.from(select.selectedOptions || []).map((option) => option.value)
    );

    select.innerHTML = '';

    if (!select.multiple) {
      const placeholder = document.createElement('option');
      placeholder.value = '';
      placeholder.textContent = '-- 请选择已打开 Tab --';
      select.appendChild(placeholder);
    }

    for (const tab of previewTabs) {
      const option = document.createElement('option');
      option.value = tab.path;
      option.textContent = `${tab.name}`;
      if (selectedValues.has(tab.path)) {
        option.selected = true;
      }
      select.appendChild(option);
    }

    if (!select.multiple && select.options.length > 0 && select.value === '') {
      if (selectedValues.size > 0) {
        const [first] = selectedValues;
        select.value = first;
      } else if (activeTabId) {
        const activeTab = getActiveTab();
        if (activeTab) {
          select.value = activeTab.path;
        }
      }
    }
  }
}

async function ensurePdfLoaded(tab) {
  if (tab.pdfDoc) {
    return tab.pdfDoc;
  }
  if (tab.loadingPromise) {
    return tab.loadingPromise;
  }

  tab.loadingPromise = (async () => {
    appendLog(`加载 PDF: ${tab.path}`);
    const bytes = await api.readFile(tab.path);
    const data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
    const loadingTask = pdfjsLib.getDocument({ data });
    tab.pdfDoc = await loadingTask.promise;
    tab.totalPages = tab.pdfDoc.numPages;
    tab.currentPage = Math.min(Math.max(tab.currentPage, 1), Math.max(1, tab.totalPages));
    return tab.pdfDoc;
  })();

  try {
    return await tab.loadingPromise;
  } finally {
    tab.loadingPromise = null;
  }
}

async function renderActiveTab() {
  const token = ++renderToken;
  const tab = getActiveTab();
  refreshPreviewLabels(tab);

  if (!tab) {
    showPreviewPlaceholder('暂无预览', '从 File 菜单打开 PDF，或在左侧工具区选择输入文件。');
    return;
  }

  try {
    await ensurePdfLoaded(tab);
    if (token !== renderToken) {
      return;
    }

    const page = await tab.pdfDoc.getPage(tab.currentPage);
    if (token !== renderToken) {
      return;
    }

    const viewport = page.getViewport({ scale: tab.scale });
    const dpr = window.devicePixelRatio || 1;

    previewCanvasEl.width = Math.max(1, Math.floor(viewport.width * dpr));
    previewCanvasEl.height = Math.max(1, Math.floor(viewport.height * dpr));
    previewCanvasEl.style.width = `${Math.floor(viewport.width)}px`;
    previewCanvasEl.style.height = `${Math.floor(viewport.height)}px`;

    const ctx = previewCanvasEl.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, viewport.width, viewport.height);

    await page.render({ canvasContext: ctx, viewport }).promise;
    if (token !== renderToken) {
      return;
    }

    showPreviewCanvas();
    refreshPreviewLabels(tab);
  } catch (error) {
    showPreviewPlaceholder('预览失败', error.message || String(error));
    appendLog(`PDF 渲染失败: ${error.message || String(error)}`);
  }
}

function setActiveTab(id) {
  const exists = previewTabs.some((tab) => tab.id === id);
  if (!exists) {
    return;
  }
  activeTabId = id;
  renderTabs();
  renderActiveTab();
}

function destroyTabDocument(tab) {
  if (tab?.pdfDoc && typeof tab.pdfDoc.destroy === 'function') {
    Promise.resolve(tab.pdfDoc.destroy()).catch(() => {});
  }
}

function closeTab(id) {
  const index = previewTabs.findIndex((tab) => tab.id === id);
  if (index < 0) {
    return;
  }

  const tab = previewTabs[index];
  const wasActive = tab.id === activeTabId;
  destroyTabDocument(tab);
  previewTabs.splice(index, 1);

  if (previewTabs.length === 0) {
    activeTabId = null;
  } else if (wasActive) {
    activeTabId = previewTabs[Math.max(0, index - 1)].id;
  }

  renderTabs();
  refreshTabSelectors();
  renderActiveTab();
}

function closeAllTabs() {
  previewTabs.forEach((tab) => destroyTabDocument(tab));
  previewTabs.length = 0;
  activeTabId = null;
  renderTabs();
  refreshTabSelectors();
  renderActiveTab();
}

function openPdfTab(filePath, focus = true) {
  if (!filePath) {
    return;
  }

  const normalizedPath = filePath.trim();
  if (!normalizedPath) {
    return;
  }

  const existing = previewTabs.find((tab) => tab.path === normalizedPath);
  if (existing) {
    if (focus) {
      setActiveTab(existing.id);
    }
    return;
  }

  const tab = {
    id: `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
    path: normalizedPath,
    name: basename(normalizedPath),
    pdfDoc: null,
    loadingPromise: null,
    totalPages: 0,
    currentPage: 1,
    scale: 1.15
  };

  previewTabs.push(tab);
  refreshTabSelectors();
  renderTabs();

  if (focus || !activeTabId) {
    activeTabId = tab.id;
    renderActiveTab();
  }
}

async function pickPdf({ multiple = false } = {}) {
  const response = await api.openDialog({
    title: multiple ? '选择 PDF 文件（可多选）' : '选择 PDF 文件',
    properties: multiple ? ['openFile', 'multiSelections'] : ['openFile'],
    filters: [{ name: 'PDF', extensions: ['pdf'] }]
  });

  if (response.canceled) {
    return [];
  }
  return response.filePaths;
}

async function pickSavePath({ title, defaultPath, extension }) {
  const response = await api.saveDialog({
    title,
    defaultPath,
    filters: [{ name: extension.toUpperCase(), extensions: [extension] }]
  });

  if (response.canceled) {
    return '';
  }
  return response.filePath;
}

async function suggestOutput(inputPath, suffix, ext) {
  return api.suggestOutput({ inputPath, suffix, ext });
}

async function runTask(task, successHint) {
  if (!ensureIdle()) {
    return null;
  }

  const backend = getActiveBackend();
  if (!backend) {
    appendLog('未找到可用 backend，任务取消。');
    return null;
  }

  setButtonsDisabled(true);
  appendLog(`开始任务: ${task.type} (backend=${backend.id})`);
  if (backend.id === BackendId.Wasm && wasmBridgeTaskTypes.has(task?.type)) {
    updateBridgeStatsView({
      status: 'pending',
      statusText: '执行中',
      task: task.type,
      stage: 'running',
      time: '--',
      files: '--',
      bytes: '--',
      avg: formatBridgeAverage(),
      success: formatBridgeSuccessRate(),
      fallback: '--'
    });
  }

  try {
    const result = await backend.runTask(task);
    if (Array.isArray(result.args) && result.args.length > 0) {
      appendLog(`命令: pdftools ${result.args.join(' ')}`);
    }

    if (result.stdout) {
      appendLog(`stdout: ${result.stdout.trim()}`);
    }
    if (result.stderr) {
      appendLog(`stderr: ${result.stderr.trim()}`);
    }

    const bridgeDetails =
      result?.details && typeof result.details === 'object' && result.details.bridge
        ? result.details
        : null;
    if (bridgeDetails) {
      const ioFiles = bridgeDetails.ioFiles || {};
      const ioBytes = bridgeDetails.ioBytes || {};
      const timingsMs = bridgeDetails.timingsMs || {};
      appendLog(
        `WASM bridge: files(in/out)=${ioFiles.input ?? 0}/${ioFiles.output ?? 0}, bytes(in/out)=${ioBytes.input ?? 0}/${ioBytes.output ?? 0}`
      );
      appendLog(
        `WASM bridge timings(ms): prepare=${timingsMs.prepareInput ?? 0}, run=${timingsMs.workerRun ?? 0}, persist=${timingsMs.persistOutput ?? 0}, total=${timingsMs.total ?? 0}`
      );
    }

    if (result.ok) {
      if (bridgeDetails) {
        const filesText = `${bridgeDetails.ioFiles?.input ?? 0} / ${bridgeDetails.ioFiles?.output ?? 0}`;
        const bytesText = `${formatBytes(bridgeDetails.ioBytes?.input ?? 0)} / ${formatBytes(
          bridgeDetails.ioBytes?.output ?? 0
        )}`;
        const totalMs = bridgeDetails.timingsMs?.total;
        recordBridgeOutcome(true);
        recordBridgeTotal(totalMs);
        updateBridgeStatsView({
          status: 'ok',
          statusText: '最近成功',
          task: bridgeDetails.taskType || task.type,
          stage: 'done',
          time: Number.isFinite(Number(totalMs)) ? `${Number(totalMs).toFixed(1)} ms` : '--',
          files: filesText,
          bytes: bytesText,
          avg: formatBridgeAverage(),
          success: formatBridgeSuccessRate(),
          fallback: '否'
        });
      }
      appendLog(`任务成功: ${successHint}`);
    } else {
      appendLog(`任务失败(code=${result.code})`);
      if (result?.details?.stage) {
        appendLog(`失败阶段: ${result.details.stage}`);
      }
      if (backend.id === BackendId.Wasm) {
        if (wasmBridgeTaskTypes.has(task?.type)) {
          recordBridgeOutcome(false);
        }
        const elapsedMs = Number(result?.details?.elapsedMs);
        updateBridgeStatsView({
          status: 'error',
          statusText: '最近失败',
          task: task?.type || '--',
          stage: result?.details?.stage || 'failed',
          time: Number.isFinite(elapsedMs) ? `${elapsedMs.toFixed(1)} ms` : '--',
          files: '--',
          bytes: '--',
          avg: formatBridgeAverage(),
          success: formatBridgeSuccessRate(),
          fallback: '否'
        });
      }
      if (backend.id === BackendId.Wasm && wasmPreviewHintCodes.has(result?.code)) {
        appendLog('提示: wasm backend 为 preview，当前仅部分能力可用。');
      }

      if (backend.id === BackendId.Wasm && wasmAutoFallbackEnabled && shouldFallbackFromWasm(result)) {
        const nativeBackend = getBackendById(BackendId.NativeCli);
        if (!nativeBackend) {
          appendLog('自动回退失败：未找到 Native CLI backend。');
          return result;
        }

        const nativeStatus = await nativeBackend.getStatus();
        if (!nativeStatus.ready) {
          appendLog(`自动回退失败：${nativeStatus.message}`);
          return result;
        }

        appendLog(`WASM 失败(code=${result.code})，尝试自动回退到 Native CLI。`);
        const fallbackResult = await nativeBackend.runTask(task);
        if (Array.isArray(fallbackResult.args) && fallbackResult.args.length > 0) {
          appendLog(`Fallback 命令: pdftools ${fallbackResult.args.join(' ')}`);
        }
        if (fallbackResult.stdout) {
          appendLog(`fallback stdout: ${fallbackResult.stdout.trim()}`);
        }
        if (fallbackResult.stderr) {
          appendLog(`fallback stderr: ${fallbackResult.stderr.trim()}`);
        }

        if (fallbackResult.ok) {
          lastFallbackAt = now();
          appendLog(`回退成功: ${successHint}`);
          updateBridgeStatsView({
            status: 'pending',
            statusText: 'WASM失败已回退',
            task: task?.type || '--',
            stage: 'fallback-native',
            time: '--',
            files: '--',
            bytes: '--',
            avg: formatBridgeAverage(),
            success: formatBridgeSuccessRate(),
            fallback: '是',
            fallbackTime: lastFallbackAt
          });
          return {
            ...fallbackResult,
            fallbackFrom: BackendId.Wasm
          };
        }

        appendLog(`回退后仍失败(code=${fallbackResult.code})`);
      }
      if (backend.id === BackendId.Wasm && !wasmAutoFallbackEnabled && shouldFallbackFromWasm(result)) {
        appendLog('WASM 自动回退已关闭，可在 Settings 中开启。');
      }
    }

    return result;
  } catch (error) {
    appendLog(`任务异常: ${error.message || String(error)}`);
    return null;
  } finally {
    setButtonsDisabled(false);
  }
}

async function initStatus() {
  await refreshBackendStatus({ log: true });
}

function bindNativeMenuBar() {
  if (typeof api.onMenuAction !== 'function') {
    appendLog('当前预加载 API 不支持原生菜单事件。');
    return;
  }

  api.onMenuAction((event) => {
    const action = event?.action;
    if (action === 'open-pdf') {
      const files = Array.isArray(event.payload) ? event.payload : [];
      if (files.length === 0) {
        return;
      }
      files.forEach((file, index) => openPdfTab(file, index === files.length - 1));
      appendLog(`File > Open: 打开 ${files.length} 个 PDF Tab`);
      return;
    }

    if (action === 'close-current-tab') {
      if (activeTabId) {
        closeTab(activeTabId);
      }
      return;
    }

    if (action === 'close-all-tabs') {
      closeAllTabs();
      return;
    }

    if (action === 'open-theme-settings') {
      openThemeSettings();
    }
  });
}

function bindBackendLifecycle() {
  window.addEventListener('beforeunload', () => {
    for (const backend of backendRegistry.values()) {
      if (typeof backend?.dispose === 'function') {
        Promise.resolve(backend.dispose()).catch(() => {});
      }
    }
  });
}

function bindPreviewToolbar() {
  document.getElementById('preview-prev-page').addEventListener('click', () => {
    const tab = getActiveTab();
    if (!tab || tab.currentPage <= 1) {
      return;
    }
    tab.currentPage -= 1;
    renderActiveTab();
  });

  document.getElementById('preview-next-page').addEventListener('click', () => {
    const tab = getActiveTab();
    if (!tab || tab.totalPages === 0 || tab.currentPage >= tab.totalPages) {
      return;
    }
    tab.currentPage += 1;
    renderActiveTab();
  });

  document.getElementById('preview-zoom-in').addEventListener('click', () => {
    const tab = getActiveTab();
    if (!tab) {
      return;
    }
    tab.scale = Math.min(3.0, tab.scale + 0.1);
    renderActiveTab();
  });

  document.getElementById('preview-zoom-out').addEventListener('click', () => {
    const tab = getActiveTab();
    if (!tab) {
      return;
    }
    tab.scale = Math.max(0.5, tab.scale - 0.1);
    renderActiveTab();
  });
}

function bindTabSelectors() {
  for (const [selectId, inputId] of singleTabSelectBindings) {
    const selectEl = document.getElementById(selectId);
    const inputEl = document.getElementById(inputId);
    if (!selectEl || !inputEl) {
      continue;
    }

    selectEl.addEventListener('change', () => {
      if (!selectEl.value) {
        return;
      }
      inputEl.value = selectEl.value;
      openPdfTab(selectEl.value, true);
    });
  }

  document.getElementById('merge-use-selected-tabs').addEventListener('click', () => {
    const selectEl = document.getElementById('merge-tab-select');
    const inputEl = document.getElementById('merge-inputs');
    const selectedPaths = Array.from(selectEl.selectedOptions)
      .map((option) => option.value)
      .filter(Boolean);

    if (selectedPaths.length === 0) {
      appendLog('未选择任何 Tab。');
      return;
    }

    inputEl.value = selectedPaths.join('\n');
    selectedPaths.forEach((path, index) => openPdfTab(path, index === selectedPaths.length - 1));
    appendLog(`已用 ${selectedPaths.length} 个 Tab 填充合并输入。`);
  });
}

function bindMerge() {
  const inputEl = document.getElementById('merge-inputs');
  const outputEl = document.getElementById('merge-output');

  document.getElementById('merge-select-inputs').addEventListener('click', async () => {
    const files = await pickPdf({ multiple: true });
    if (files.length === 0) {
      return;
    }

    inputEl.value = files.join('\n');
    files.forEach((file, index) => openPdfTab(file, index === files.length - 1));

    if (!outputEl.value) {
      outputEl.value = await suggestOutput(files[0], '_merged', '.pdf');
    }
  });

  document.getElementById('merge-select-output').addEventListener('click', async () => {
    const inputs = parseMultiLinePaths(inputEl.value);
    const defaultPath = outputEl.value || (inputs[0] ? await suggestOutput(inputs[0], '_merged', '.pdf') : undefined);
    const output = await pickSavePath({ title: '保存合并 PDF', defaultPath, extension: 'pdf' });
    if (output) {
      outputEl.value = output;
    }
  });

  document.getElementById('merge-run').addEventListener('click', async () => {
    const inputPdfs = parseMultiLinePaths(inputEl.value);
    const outputPdf = outputEl.value.trim();
    if (inputPdfs.length < 2) {
      appendLog('合并任务至少需要 2 个输入 PDF。');
      return;
    }
    if (!outputPdf) {
      appendLog('请先选择合并输出文件。');
      return;
    }
    const result = await runTask({ type: 'merge', inputPdfs, outputPdf }, `合并完成: ${outputPdf}`);
    if (result?.ok && outputPdf) {
      openPdfTab(outputPdf, true);
    }
  });
}

function bindDelete() {
  const inputEl = document.getElementById('delete-input');
  const outputEl = document.getElementById('delete-output');
  const pageEl = document.getElementById('delete-page');

  document.getElementById('delete-select-input').addEventListener('click', async () => {
    const [file] = await pickPdf();
    if (!file) {
      return;
    }
    inputEl.value = file;
    openPdfTab(file, true);
    if (!outputEl.value) {
      outputEl.value = await suggestOutput(file, '_delete', '.pdf');
    }
  });

  document.getElementById('delete-select-output').addEventListener('click', async () => {
    const defaultPath = outputEl.value || (inputEl.value ? await suggestOutput(inputEl.value, '_delete', '.pdf') : undefined);
    const output = await pickSavePath({ title: '保存删除页后的 PDF', defaultPath, extension: 'pdf' });
    if (output) {
      outputEl.value = output;
    }
  });

  document.getElementById('delete-run').addEventListener('click', async () => {
    const outputPdf = outputEl.value.trim();
    const result = await runTask(
      {
        type: 'delete-page',
        inputPdf: inputEl.value.trim(),
        outputPdf,
        page: Number(pageEl.value)
      },
      `删除页完成: ${outputPdf}`
    );
    if (result?.ok && outputPdf) {
      openPdfTab(outputPdf, true);
    }
  });
}

function bindInsert() {
  const targetEl = document.getElementById('insert-target');
  const sourceEl = document.getElementById('insert-source');
  const outputEl = document.getElementById('insert-output');
  const atEl = document.getElementById('insert-at');
  const sourcePageEl = document.getElementById('insert-source-page');

  document.getElementById('insert-select-target').addEventListener('click', async () => {
    const [file] = await pickPdf();
    if (!file) {
      return;
    }
    targetEl.value = file;
    openPdfTab(file, true);
    if (!outputEl.value) {
      outputEl.value = await suggestOutput(file, '_insert', '.pdf');
    }
  });

  document.getElementById('insert-select-source').addEventListener('click', async () => {
    const [file] = await pickPdf();
    if (!file) {
      return;
    }
    sourceEl.value = file;
    openPdfTab(file, true);
  });

  document.getElementById('insert-select-output').addEventListener('click', async () => {
    const defaultPath = outputEl.value || (targetEl.value ? await suggestOutput(targetEl.value, '_insert', '.pdf') : undefined);
    const output = await pickSavePath({ title: '保存插页后的 PDF', defaultPath, extension: 'pdf' });
    if (output) {
      outputEl.value = output;
    }
  });

  document.getElementById('insert-run').addEventListener('click', async () => {
    const outputPdf = outputEl.value.trim();
    const result = await runTask(
      {
        type: 'insert-page',
        inputPdf: targetEl.value.trim(),
        sourcePdf: sourceEl.value.trim(),
        outputPdf,
        at: Number(atEl.value),
        sourcePage: Number(sourcePageEl.value)
      },
      `插页完成: ${outputPdf}`
    );
    if (result?.ok && outputPdf) {
      openPdfTab(outputPdf, true);
    }
  });
}

function bindReplace() {
  const targetEl = document.getElementById('replace-target');
  const sourceEl = document.getElementById('replace-source');
  const outputEl = document.getElementById('replace-output');
  const pageEl = document.getElementById('replace-page');
  const sourcePageEl = document.getElementById('replace-source-page');

  document.getElementById('replace-select-target').addEventListener('click', async () => {
    const [file] = await pickPdf();
    if (!file) {
      return;
    }
    targetEl.value = file;
    openPdfTab(file, true);
    if (!outputEl.value) {
      outputEl.value = await suggestOutput(file, '_replace', '.pdf');
    }
  });

  document.getElementById('replace-select-source').addEventListener('click', async () => {
    const [file] = await pickPdf();
    if (!file) {
      return;
    }
    sourceEl.value = file;
    openPdfTab(file, true);
  });

  document.getElementById('replace-select-output').addEventListener('click', async () => {
    const defaultPath = outputEl.value || (targetEl.value ? await suggestOutput(targetEl.value, '_replace', '.pdf') : undefined);
    const output = await pickSavePath({ title: '保存替换页后的 PDF', defaultPath, extension: 'pdf' });
    if (output) {
      outputEl.value = output;
    }
  });

  document.getElementById('replace-run').addEventListener('click', async () => {
    const outputPdf = outputEl.value.trim();
    const result = await runTask(
      {
        type: 'replace-page',
        inputPdf: targetEl.value.trim(),
        sourcePdf: sourceEl.value.trim(),
        outputPdf,
        page: Number(pageEl.value),
        sourcePage: Number(sourcePageEl.value)
      },
      `替换页完成: ${outputPdf}`
    );
    if (result?.ok && outputPdf) {
      openPdfTab(outputPdf, true);
    }
  });
}

function bindPdf2Docx() {
  const inputEl = document.getElementById('docx-input');
  const outputEl = document.getElementById('docx-output');
  const dumpIrEl = document.getElementById('docx-dump-ir');
  const noImagesEl = document.getElementById('docx-no-images');
  const anchoredEl = document.getElementById('docx-anchored');

  document.getElementById('docx-select-input').addEventListener('click', async () => {
    const [file] = await pickPdf();
    if (!file) {
      return;
    }
    inputEl.value = file;
    openPdfTab(file, true);

    if (!outputEl.value) {
      outputEl.value = await suggestOutput(file, '_converted', '.docx');
    }
    if (!dumpIrEl.value) {
      dumpIrEl.value = await suggestOutput(file, '_ir_dump', '.json');
    }
  });

  document.getElementById('docx-select-output').addEventListener('click', async () => {
    const defaultPath = outputEl.value || (inputEl.value ? await suggestOutput(inputEl.value, '_converted', '.docx') : undefined);
    const output = await pickSavePath({ title: '保存 DOCX', defaultPath, extension: 'docx' });
    if (output) {
      outputEl.value = output;
    }
  });

  document.getElementById('docx-run').addEventListener('click', async () => {
    const outputDocx = outputEl.value.trim();
    await runTask(
      {
        type: 'pdf2docx',
        inputPdf: inputEl.value.trim(),
        outputDocx,
        noImages: noImagesEl.checked,
        anchoredImages: anchoredEl.checked,
        dumpIrPath: dumpIrEl.value.trim()
      },
      `PDF -> DOCX 完成: ${outputDocx}`
    );
  });
}

function bootstrap() {
  initThemeSettings();
  initBackendSettings();
  initWasmFallbackSettings();
  resetBridgeStats({ log: false });
  appendLog('UI 已初始化。');
  initStatus().catch((error) => {
    statusEl.className = 'status error';
    statusEl.textContent = '检测 backend 失败';
    appendLog(`backend 检测失败: ${error.message || String(error)}`);
  });

  bindNativeMenuBar();
  bindBackendLifecycle();
  bindThemeSettings();
  bindBridgeStatsControls();
  bindPreviewToolbar();
  bindTabSelectors();

  bindMerge();
  bindDelete();
  bindInsert();
  bindReplace();
  bindPdf2Docx();

  renderTabs();
  refreshTabSelectors();
  renderActiveTab();
}

bootstrap();
