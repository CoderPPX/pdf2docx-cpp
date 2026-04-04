import { createWorkerDispatcher } from './worker_dispatcher.mjs';

const dispatcher = createWorkerDispatcher();

self.addEventListener('message', (event) => {
  void dispatcher.handleMessage(event.data, (response) => {
    self.postMessage(response);
  });
});

self.postMessage(dispatcher.createReadyEvent());
