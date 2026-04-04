import { parentPort, workerData } from 'node:worker_threads';

import { createWorkerDispatcher } from './worker_dispatcher.mjs';
import { createMockPdftoolsBackend, createPdftoolsWasmBackend } from './pdftools_wasm_backend.mjs';

if (!parentPort) {
  throw new Error('node worker requires parentPort');
}

const backendFactory = workerData?.backend === 'mock'
  ? createMockPdftoolsBackend
  : createPdftoolsWasmBackend;

const dispatcher = createWorkerDispatcher({ backendFactory });

parentPort.on('message', (message) => {
  void dispatcher.handleMessage(message, (response) => {
    parentPort.postMessage(response);
  });
});

parentPort.postMessage(dispatcher.createReadyEvent());
