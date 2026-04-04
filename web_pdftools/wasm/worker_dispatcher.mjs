import {
  WorkerCommand,
  WorkerProtocolError,
  ensureRequestShape,
  makeErrorResponse,
  makeSuccessResponse,
  makeEventMessage
} from './worker_protocol.mjs';
import { createPdftoolsWasmBackend } from './pdftools_wasm_backend.mjs';

export function createWorkerDispatcher(options = {}) {
  const backendFactory = options.backendFactory || createPdftoolsWasmBackend;

  let backend = null;
  let initialized = false;

  async function initBackend(payload = {}) {
    if (initialized && backend) {
      return {
        initialized: true,
        reused: true,
        backend: backend.name || 'unknown'
      };
    }

    backend = await backendFactory(payload);
    if (!backend || typeof backend.init !== 'function' || typeof backend.run !== 'function') {
      throw new WorkerProtocolError('BACKEND_INVALID', 'backendFactory must return object with init/run methods');
    }

    const backendInfo = await backend.init(payload);
    initialized = true;
    return {
      initialized: true,
      reused: false,
      backend: backend.name || 'unknown',
      backendInfo: backendInfo || null
    };
  }

  async function runOperation(payload = {}) {
    if (!initialized || !backend) {
      throw new WorkerProtocolError('NOT_INITIALIZED', 'Worker backend is not initialized');
    }

    return backend.run(payload);
  }

  async function shutdownBackend() {
    if (backend && typeof backend.dispose === 'function') {
      await backend.dispose();
    }

    backend = null;
    initialized = false;
    return { shutdown: true };
  }

  async function dispatch(request) {
    const normalized = ensureRequestShape(request);
    const { id, command, payload = {} } = normalized;

    try {
      if (command === WorkerCommand.Init) {
        const result = await initBackend(payload);
        return makeSuccessResponse(id, result);
      }

      if (command === WorkerCommand.Run) {
        const result = await runOperation(payload);
        return makeSuccessResponse(id, result);
      }

      if (command === WorkerCommand.Ping) {
        return makeSuccessResponse(id, {
          pong: true,
          initialized
        });
      }

      if (command === WorkerCommand.Shutdown) {
        const result = await shutdownBackend();
        return makeSuccessResponse(id, result);
      }

      throw new WorkerProtocolError('UNSUPPORTED_COMMAND', `Unsupported command: ${command}`);
    } catch (error) {
      return makeErrorResponse(id, error);
    }
  }

  async function handleMessage(message, postMessage) {
    let response;
    let requestId = null;

    try {
      if (message && typeof message === 'object' && typeof message.id === 'string') {
        requestId = message.id;
      }

      response = await dispatch(message);
    } catch (error) {
      response = makeErrorResponse(requestId, error);
    }

    postMessage(response);
  }

  function createReadyEvent() {
    return makeEventMessage('ready', {
      initialized,
      backend: initialized && backend ? backend.name || 'unknown' : null
    });
  }

  return {
    dispatch,
    handleMessage,
    createReadyEvent
  };
}
