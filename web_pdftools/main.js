const { app, BrowserWindow, Menu, dialog, ipcMain } = require('electron');
const { spawn, spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');

function createWindow() {
  const win = new BrowserWindow({
    width: 1220,
    height: 860,
    minWidth: 1024,
    minHeight: 700,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  });

  buildNativeMenu(win);
  win.loadFile(path.join(__dirname, 'index.html'));
}

function sendMenuAction(win, action, payload = null) {
  if (!win || win.isDestroyed()) {
    return;
  }
  win.webContents.send('menu:action', { action, payload });
}

async function openPdfFromNativeMenu(win) {
  if (!win || win.isDestroyed()) {
    return;
  }

  const result = await dialog.showOpenDialog(win, {
    title: 'Open PDF',
    properties: ['openFile', 'multiSelections'],
    filters: [{ name: 'PDF', extensions: ['pdf'] }]
  });

  if (!result.canceled && result.filePaths.length > 0) {
    sendMenuAction(win, 'open-pdf', result.filePaths);
  }
}

function buildNativeMenu(initialWindow) {
  const isMac = process.platform === 'darwin';
  const targetWindow = () => BrowserWindow.getFocusedWindow() || initialWindow;

  const template = [];

  if (isMac) {
    template.push({ role: 'appMenu' });
  }

  template.push({
    label: 'File',
    submenu: [
      {
        label: 'Open PDF...',
        accelerator: 'CmdOrCtrl+O',
        click: () => {
          void openPdfFromNativeMenu(targetWindow());
        }
      },
      {
        label: 'Close Current Tab',
        accelerator: 'CmdOrCtrl+W',
        click: () => {
          sendMenuAction(targetWindow(), 'close-current-tab');
        }
      },
      {
        label: 'Close All Tabs',
        accelerator: 'Shift+CmdOrCtrl+W',
        click: () => {
          sendMenuAction(targetWindow(), 'close-all-tabs');
        }
      },
      { type: 'separator' },
      isMac ? { role: 'close' } : { role: 'quit' }
    ]
  });

  template.push({ role: 'viewMenu' });

  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}

function getBundledBinaryName() {
  return process.platform === 'win32' ? 'pdftools.exe' : 'pdftools';
}

function getBinaryCandidates() {
  const binaryName = getBundledBinaryName();
  const candidates = [];

  if (process.env.PDFTOOLS_BIN) {
    candidates.push(process.env.PDFTOOLS_BIN);
  }

  candidates.push(path.join(__dirname, '..', 'cpp_pdftools', 'build', 'linux-release', binaryName));
  candidates.push(path.join(__dirname, '..', 'cpp_pdftools', 'build', 'linux-debug', binaryName));
  candidates.push(path.join(__dirname, '..', 'cpp_pdftools', 'build', binaryName));

  if (process.platform === 'darwin') {
    candidates.push(path.join(__dirname, '..', 'cpp_pdftools', 'build', 'macos-release', binaryName));
    candidates.push(path.join(__dirname, '..', 'cpp_pdftools', 'build', 'macos-debug', binaryName));
  }

  if (process.platform === 'win32') {
    candidates.push(path.join(__dirname, '..', 'cpp_pdftools', 'build', 'windows-release', binaryName));
    candidates.push(path.join(__dirname, '..', 'cpp_pdftools', 'build', 'windows-debug', binaryName));
  }

  return candidates;
}

function resolvePdftoolsBinary() {
  for (const candidate of getBinaryCandidates()) {
    if (!candidate) {
      continue;
    }
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  const command = process.platform === 'win32' ? 'where' : 'which';
  const result = spawnSync(command, ['pdftools'], { encoding: 'utf8' });
  if (result.status === 0 && result.stdout) {
    const lines = result.stdout
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter(Boolean);
    if (lines.length > 0) {
      return lines[0];
    }
  }

  return null;
}

function runPdftools(args) {
  return new Promise((resolve) => {
    const binary = resolvePdftoolsBinary();
    if (!binary) {
      resolve({
        ok: false,
        code: -1,
        stdout: '',
        stderr:
          'pdftools binary not found. Build cpp_pdftools first or set PDFTOOLS_BIN to your binary path.'
      });
      return;
    }

    const child = spawn(binary, args, {
      stdio: ['ignore', 'pipe', 'pipe'],
      shell: false
    });

    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (chunk) => {
      stdout += chunk.toString();
    });

    child.stderr.on('data', (chunk) => {
      stderr += chunk.toString();
    });

    child.on('error', (error) => {
      resolve({
        ok: false,
        code: -1,
        stdout,
        stderr: `${stderr}\n${error.message}`.trim()
      });
    });

    child.on('close', (code) => {
      resolve({
        ok: code === 0,
        code: code ?? -1,
        stdout,
        stderr,
        binary
      });
    });
  });
}

function buildArgsFromTask(task) {
  if (!task || typeof task !== 'object') {
    throw new Error('task payload is empty');
  }

  if (task.type === 'merge') {
    if (!Array.isArray(task.inputPdfs) || task.inputPdfs.length < 2 || !task.outputPdf) {
      throw new Error('merge requires at least 2 input PDFs and an output path');
    }
    return ['merge', task.outputPdf, ...task.inputPdfs];
  }

  if (task.type === 'delete-page') {
    if (!task.inputPdf || !task.outputPdf || !task.page) {
      throw new Error('delete-page requires inputPdf/outputPdf/page');
    }
    return [
      'page',
      'delete',
      '--input',
      task.inputPdf,
      '--output',
      task.outputPdf,
      '--page',
      String(task.page)
    ];
  }

  if (task.type === 'insert-page') {
    if (!task.inputPdf || !task.outputPdf || !task.sourcePdf || !task.at || !task.sourcePage) {
      throw new Error('insert-page requires input/output/source/at/sourcePage');
    }
    return [
      'page',
      'insert',
      '--input',
      task.inputPdf,
      '--output',
      task.outputPdf,
      '--at',
      String(task.at),
      '--source',
      task.sourcePdf,
      '--source-page',
      String(task.sourcePage)
    ];
  }

  if (task.type === 'replace-page') {
    if (!task.inputPdf || !task.outputPdf || !task.sourcePdf || !task.page || !task.sourcePage) {
      throw new Error('replace-page requires input/output/source/page/sourcePage');
    }
    return [
      'page',
      'replace',
      '--input',
      task.inputPdf,
      '--output',
      task.outputPdf,
      '--page',
      String(task.page),
      '--source',
      task.sourcePdf,
      '--source-page',
      String(task.sourcePage)
    ];
  }

  if (task.type === 'pdf2docx') {
    if (!task.inputPdf || !task.outputDocx) {
      throw new Error('pdf2docx requires inputPdf/outputDocx');
    }
    const args = [
      'convert',
      'pdf2docx',
      '--input',
      task.inputPdf,
      '--output',
      task.outputDocx
    ];

    if (task.noImages) {
      args.push('--no-images');
    }
    if (task.anchoredImages) {
      args.push('--docx-anchored');
    }
    if (task.dumpIrPath) {
      args.push('--dump-ir', task.dumpIrPath);
    }

    return args;
  }

  throw new Error(`unsupported task type: ${task.type}`);
}

function defaultOutputName(inputPath, suffix, ext) {
  const parsed = path.parse(inputPath || 'output.pdf');
  return `${parsed.name}${suffix}${ext}`;
}

ipcMain.handle('dialog:open', async (_event, payload) => {
  const result = await dialog.showOpenDialog({
    title: payload?.title || 'Select file',
    properties: payload?.properties || ['openFile'],
    filters: payload?.filters || [{ name: 'All Files', extensions: ['*'] }]
  });

  if (result.canceled) {
    return { canceled: true, filePaths: [] };
  }

  return { canceled: false, filePaths: result.filePaths };
});

ipcMain.handle('dialog:save', async (_event, payload) => {
  const result = await dialog.showSaveDialog({
    title: payload?.title || 'Save file',
    defaultPath: payload?.defaultPath,
    filters: payload?.filters || [{ name: 'All Files', extensions: ['*'] }]
  });

  if (result.canceled || !result.filePath) {
    return { canceled: true };
  }

  return { canceled: false, filePath: result.filePath };
});

ipcMain.handle('pdftools:status', async () => {
  const binary = resolvePdftoolsBinary();
  return {
    found: Boolean(binary),
    binaryPath: binary || '',
    hint:
      binary ||
      'Build cpp_pdftools first (cmake --preset linux-release && cmake --build --preset linux-release) or set PDFTOOLS_BIN.'
  };
});

ipcMain.handle('pdftools:run', async (_event, task) => {
  let args;
  try {
    args = buildArgsFromTask(task);
  } catch (error) {
    return {
      ok: false,
      code: -1,
      stdout: '',
      stderr: error.message || String(error)
    };
  }

  const result = await runPdftools(args);
  return {
    ...result,
    args
  };
});

ipcMain.handle('paths:suggestOutput', async (_event, payload) => {
  const inputPath = payload?.inputPath || '';
  const suffix = payload?.suffix || '_out';
  const ext = payload?.ext || '.pdf';
  const dir = inputPath ? path.dirname(inputPath) : app.getPath('documents');
  const fileName = defaultOutputName(inputPath, suffix, ext);
  return path.join(dir, fileName);
});

ipcMain.handle('fs:readFile', async (_event, filePath) => {
  if (!filePath || typeof filePath !== 'string') {
    throw new Error('invalid file path');
  }

  const normalized = path.resolve(filePath);
  return fs.promises.readFile(normalized);
});

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});
