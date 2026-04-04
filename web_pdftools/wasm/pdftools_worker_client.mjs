import { WorkerCommand, WorkerProtocolError, isPlainObject } from './worker_protocol.mjs';

function createId(prefix = 'req') {
  return `${prefix}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
}

function createTimeoutError(timeoutMs) {
  return new WorkerProtocolError('TIMEOUT', `Worker request timed out after ${timeoutMs}ms`, {
    timeoutMs
  }, 'worker.request');
}

function toClientError(errorPayload) {
  if (isPlainObject(errorPayload)) {
    return new WorkerProtocolError(
      typeof errorPayload.code === 'string' ? errorPayload.code : 'WORKER_ERROR',
      typeof errorPayload.message === 'string' ? errorPayload.message : 'Worker request failed',
      'details' in errorPayload ? errorPayload.details : null,
      typeof errorPayload.context === 'string' ? errorPayload.context : null
    );
  }

  return new WorkerProtocolError('WORKER_ERROR', 'Worker request failed', null);
}

function defaultWorkerFactory(workerUrl) {
  if (typeof Worker !== 'function') {
    throw new WorkerProtocolError(
      'WORKER_UNAVAILABLE',
      'Global Worker is not available. Pass a custom workerFactory in this environment.'
    );
  }

  return new Worker(workerUrl, { type: 'module' });
}

export class PdftoolsWorkerClient {
  constructor(options = {}) {
    const workerUrl = options.workerUrl || new URL('./pdftools.worker.mjs', import.meta.url);
    this._workerUrl = workerUrl;
    this._workerFactory = options.workerFactory || defaultWorkerFactory;
    this._requestTimeoutMs = Number(options.requestTimeoutMs ?? 30_000);

    this._worker = null;
    this._pending = new Map();
    this._started = false;
  }

  async start(initPayload = {}) {
    this._ensureWorker();
    const payload = isPlainObject(initPayload) ? initPayload : {};
    const result = await this._request(WorkerCommand.Init, payload);
    this._started = true;
    return result;
  }

  async run(operationPayload = {}, options = {}) {
    const timeoutMs = Number(options.timeoutMs ?? this._requestTimeoutMs);
    return this._request(WorkerCommand.Run, operationPayload, timeoutMs);
  }

  async ping() {
    return this._request(WorkerCommand.Ping, {});
  }

  async shutdown() {
    const result = await this._request(WorkerCommand.Shutdown, {});
    this._started = false;
    return result;
  }

  terminate() {
    if (!this._worker) {
      return;
    }

    this._terminateWorker();
    this._rejectAllPending(
      new WorkerProtocolError('WORKER_TERMINATED', 'Worker was terminated by client')
    );
    this._started = false;
  }

  _ensureWorker() {
    if (this._worker) {
      return;
    }

    this._worker = this._workerFactory(this._workerUrl);
    this._bindWorker(this._worker);
  }

  _bindWorker(worker) {
    const onMessage = (message) => {
      this._handleWorkerMessage(message);
    };

    const onError = (error) => {
      const wrapped = new WorkerProtocolError('WORKER_RUNTIME_ERROR', error?.message || 'Worker runtime error');
      this._rejectAllPending(wrapped);
    };

    const onExit = () => {
      const wrapped = new WorkerProtocolError('WORKER_CLOSED', 'Worker exited unexpectedly');
      this._rejectAllPending(wrapped);
      this._worker = null;
      this._started = false;
    };

    // Browser Worker
    if (typeof worker.addEventListener === 'function') {
      worker.addEventListener('message', (event) => onMessage(event.data));
      worker.addEventListener('error', onError);
      return;
    }

    // Node worker_threads Worker
    if (typeof worker.on === 'function') {
      worker.on('message', onMessage);
      worker.on('error', onError);
      worker.on('exit', onExit);
      return;
    }

    throw new WorkerProtocolError('WORKER_BIND_FAILED', 'Unsupported worker instance');
  }

  _request(command, payload = {}, timeoutMs = this._requestTimeoutMs) {
    this._ensureWorker();
    const id = createId(command);

    const timeout = Number(timeoutMs);
    const requestTimeoutMs = Number.isFinite(timeout) && timeout > 0 ? timeout : this._requestTimeoutMs;

    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this._pending.delete(id);
        reject(createTimeoutError(requestTimeoutMs));
      }, requestTimeoutMs);

      this._pending.set(id, { resolve, reject, timer });

      this._postMessage({
        id,
        command,
        payload: isPlainObject(payload) ? payload : {}
      });
    });
  }

  _postMessage(message) {
    if (!this._worker) {
      throw new WorkerProtocolError('WORKER_UNAVAILABLE', 'Worker is not available');
    }

    if (typeof this._worker.postMessage !== 'function') {
      throw new WorkerProtocolError('WORKER_BIND_FAILED', 'Worker does not expose postMessage()');
    }

    this._worker.postMessage(message);
  }

  _handleWorkerMessage(message) {
    if (isPlainObject(message) && message.type === 'event') {
      return;
    }

    if (!isPlainObject(message) || typeof message.id !== 'string') {
      return;
    }

    const pending = this._pending.get(message.id);
    if (!pending) {
      return;
    }

    this._pending.delete(message.id);
    clearTimeout(pending.timer);

    if (message.ok) {
      pending.resolve(message.payload);
      return;
    }

    pending.reject(toClientError(message.error));
  }

  _rejectAllPending(error) {
    for (const [id, pending] of this._pending.entries()) {
      this._pending.delete(id);
      clearTimeout(pending.timer);
      pending.reject(error);
    }
  }

  _terminateWorker() {
    if (!this._worker) {
      return;
    }

    const worker = this._worker;
    this._worker = null;

    if (typeof worker.terminate === 'function') {
      worker.terminate();
    }
  }
}

export function createPdftoolsWorkerClient(options = {}) {
  return new PdftoolsWorkerClient(options);
}
