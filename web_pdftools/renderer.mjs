import * as pdfjsLib from './node_modules/pdfjs-dist/legacy/build/pdf.mjs';

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
let activeTabId = null;
let running = false;
let renderToken = 0;

function now() {
  return new Date().toLocaleTimeString();
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

  setButtonsDisabled(true);
  appendLog(`开始任务: ${task.type}`);

  try {
    const result = await api.runTask(task);
    appendLog(`命令: pdftools ${(result.args || []).join(' ')}`);

    if (result.stdout) {
      appendLog(`stdout: ${result.stdout.trim()}`);
    }
    if (result.stderr) {
      appendLog(`stderr: ${result.stderr.trim()}`);
    }

    if (result.ok) {
      appendLog(`任务成功: ${successHint}`);
    } else {
      appendLog(`任务失败(code=${result.code})`);
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
  const status = await api.getStatus();
  if (status.found) {
    statusEl.className = 'status ok';
    statusEl.textContent = `已找到 pdftools: ${status.binaryPath}`;
    appendLog(`pdftools binary: ${status.binaryPath}`);
  } else {
    statusEl.className = 'status error';
    statusEl.textContent = '未找到 pdftools';
    appendLog(status.hint);
  }
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
  appendLog('UI 已初始化。');
  initStatus().catch((error) => {
    statusEl.className = 'status error';
    statusEl.textContent = '检测 pdftools 失败';
    appendLog(`检测失败: ${error.message || String(error)}`);
  });

  bindNativeMenuBar();
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
