import { createWorkerDispatcher } from './worker_dispatcher.mjs';

const dispatcher = createWorkerDispatcher();

self.addEventListener('message', (event) => {
  void dispatcher.handleMessage(event.data, (response, transferables = []) => {
    self.postMessage(response, transferables);
  });
});

self.postMessage(dispatcher.createReadyEvent());
