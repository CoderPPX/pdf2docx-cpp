const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('pdftoolsAPI', {
  openDialog: (payload) => ipcRenderer.invoke('dialog:open', payload),
  saveDialog: (payload) => ipcRenderer.invoke('dialog:save', payload),
  getStatus: () => ipcRenderer.invoke('pdftools:status'),
  runTask: (task) => ipcRenderer.invoke('pdftools:run', task),
  suggestOutput: (payload) => ipcRenderer.invoke('paths:suggestOutput', payload),
  readFile: (filePath) => ipcRenderer.invoke('fs:readFile', filePath),
  writeFile: (payload) => ipcRenderer.invoke('fs:writeFile', payload),
  onMenuAction: (handler) => {
    if (typeof handler !== 'function') {
      return () => {};
    }
    const listener = (_event, payload) => {
      handler(payload);
    };
    ipcRenderer.on('menu:action', listener);
    return () => {
      ipcRenderer.removeListener('menu:action', listener);
    };
  }
});
