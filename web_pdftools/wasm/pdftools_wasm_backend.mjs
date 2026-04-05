import { WorkerProtocolError, isPlainObject, statusToErrorPayload, toErrorPayload } from './worker_protocol.mjs';

const encoder = new TextEncoder();
const decoder = new TextDecoder();

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function normalizeModuleFactory(moduleNamespace) {
  if (typeof moduleNamespace === 'function') {
    return moduleNamespace;
  }

  if (isPlainObject(moduleNamespace) && typeof moduleNamespace.default === 'function') {
    return moduleNamespace.default;
  }

  if (isPlainObject(moduleNamespace) && isPlainObject(moduleNamespace.default)) {
    return () => moduleNamespace.default;
  }

  if (isPlainObject(moduleNamespace)) {
    return () => moduleNamespace;
  }

  throw new WorkerProtocolError('WASM_LOAD_FAILED', 'Unsupported wasm module format');
}

function resolveExport(module, names) {
  for (const name of names) {
    if (typeof module[name] === 'function') {
      return module[name].bind(module);
    }
  }
  return null;
}

function requireEmscriptenApi(module, property) {
  if (!module || typeof module[property] !== 'function') {
    throw new WorkerProtocolError('WASM_API_MISSING', `Missing emscripten API: ${property}`);
  }
  return module[property].bind(module);
}

function requireHeap(module, property) {
  if (!module || (!(module[property] instanceof Uint8Array) && !(module[property] instanceof Uint32Array))) {
    throw new WorkerProtocolError('WASM_API_MISSING', `Missing emscripten heap: ${property}`);
  }
  return module[property];
}

function toUint8Array(data) {
  if (data instanceof Uint8Array) {
    return data;
  }
  if (data instanceof ArrayBuffer) {
    return new Uint8Array(data);
  }
  if (ArrayBuffer.isView(data)) {
    return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  }
  if (Array.isArray(data)) {
    return Uint8Array.from(data);
  }
  throw new WorkerProtocolError('BAD_REQUEST', 'Binary payload must be Uint8Array/ArrayBuffer');
}

function isHostFsBridgePayload(payload) {
  return isPlainObject(payload) && payload.__hostFsBridge === true;
}

function ensureFsApi(module) {
  const fsApi = module?.FS;
  if (!fsApi || typeof fsApi.writeFile !== 'function' || typeof fsApi.readFile !== 'function') {
    throw new WorkerProtocolError('WASM_API_MISSING', 'WASM module FS API is required for host file bridge');
  }
  return fsApi;
}

function pathParent(filePath) {
  if (typeof filePath !== 'string' || filePath.length === 0) {
    return '/';
  }
  const normalized = filePath.replace(/\\/g, '/');
  const idx = normalized.lastIndexOf('/');
  if (idx <= 0) {
    return '/';
  }
  return normalized.slice(0, idx);
}

function fsExists(fsApi, dirPath) {
  if (typeof fsApi.analyzePath === 'function') {
    try {
      return Boolean(fsApi.analyzePath(dirPath)?.exists);
    } catch (_error) {
      return false;
    }
  }

  if (typeof fsApi.stat === 'function') {
    try {
      fsApi.stat(dirPath);
      return true;
    } catch (_error) {
      return false;
    }
  }

  return false;
}

function ensureParentDir(fsApi, filePath) {
  const parent = pathParent(filePath);
  if (!parent || parent === '/') {
    return;
  }

  const segments = parent.split('/').filter(Boolean);
  let current = '';
  for (const segment of segments) {
    current += `/${segment}`;
    if (fsExists(fsApi, current)) {
      continue;
    }
    if (typeof fsApi.mkdir !== 'function') {
      continue;
    }
    try {
      fsApi.mkdir(current);
    } catch (error) {
      if (!fsExists(fsApi, current)) {
        throw new WorkerProtocolError(
          'IO_ERROR',
          `Failed to create wasm directory: ${current}`,
          { cause: String(error) },
          'wasm.fs.mkdir'
        );
      }
    }
  }
}

function stageBridgeInputs(fsApi, inputFiles) {
  if (!Array.isArray(inputFiles)) {
    return;
  }

  for (const file of inputFiles) {
    if (!isPlainObject(file) || typeof file.path !== 'string' || file.path.length === 0) {
      throw new WorkerProtocolError('BAD_REQUEST', 'Bridge input file requires path');
    }
    if (!('data' in file)) {
      throw new WorkerProtocolError('BAD_REQUEST', `Bridge input file missing data: ${file.path}`);
    }

    ensureParentDir(fsApi, file.path);
    fsApi.writeFile(file.path, toUint8Array(file.data));
  }
}

function collectBridgeOutputs(fsApi, outputFiles) {
  if (!Array.isArray(outputFiles) || outputFiles.length === 0) {
    return [];
  }

  const outputs = [];
  for (const file of outputFiles) {
    if (!isPlainObject(file) || typeof file.path !== 'string' || file.path.length === 0) {
      throw new WorkerProtocolError('BAD_REQUEST', 'Bridge output file requires path');
    }

    let data;
    try {
      data = fsApi.readFile(file.path);
    } catch (error) {
      throw new WorkerProtocolError(
        'IO_ERROR',
        `Failed to read wasm output file: ${file.path}`,
        { cause: String(error) },
        'wasm.fs.readFile'
      );
    }

    outputs.push({
      path: file.path,
      hostPath: typeof file.hostPath === 'string' ? file.hostPath : null,
      // Always return a detached-safe copy. Some FS implementations may
      // return a view backed by wasm HEAP; transferring that buffer would
      // detach the worker heap and break subsequent wasm calls.
      data: toUint8Array(data).slice()
    });
  }
  return outputs;
}

function createWasmInvoker(module) {
  const malloc = requireEmscriptenApi(module, '_malloc');
  const free = requireEmscriptenApi(module, '_free');
  const getHeapU8 = () => requireHeap(module, 'HEAPU8');
  const getHeapU32 = () => requireHeap(module, 'HEAPU32');

  const opFn = resolveExport(module, ['_pdftools_wasm_op', 'pdftools_wasm_op']);
  const freeResponseFn = resolveExport(module, ['_pdftools_wasm_free', 'pdftools_wasm_free']);

  if (!opFn || !freeResponseFn) {
    throw new WorkerProtocolError(
      'WASM_API_MISSING',
      'Missing exported C ABI functions: pdftools_wasm_op / pdftools_wasm_free'
    );
  }

  return (requestObject) => {
    const requestBytes = encoder.encode(JSON.stringify(requestObject));
    const requestPtr = malloc(requestBytes.byteLength);
    const responsePtrPtr = malloc(4);
    const responseLenPtr = malloc(4);

    try {
      // Always use fresh heap views. Wasm memory growth detaches old buffers.
      let heapU8 = getHeapU8();
      let heapU32 = getHeapU32();
      heapU8.set(requestBytes, requestPtr);
      heapU32[responsePtrPtr >>> 2] = 0;
      heapU32[responseLenPtr >>> 2] = 0;

      const statusCode = opFn(requestPtr, requestBytes.byteLength, responsePtrPtr, responseLenPtr);
      if (statusCode !== 0) {
        throw new WorkerProtocolError(
          'WASM_OP_FAILED',
          `pdftools_wasm_op returned non-zero status: ${statusCode}`,
          { statusCode },
          'pdftools_wasm_op'
        );
      }

      // Re-acquire views after op call; op may grow memory.
      heapU8 = getHeapU8();
      heapU32 = getHeapU32();
      const responsePtr = heapU32[responsePtrPtr >>> 2] >>> 0;
      const responseLen = heapU32[responseLenPtr >>> 2] >>> 0;

      if (responsePtr === 0 || responseLen === 0) {
        throw new WorkerProtocolError('WASM_OP_FAILED', 'WASM response buffer is empty');
      }

      const responseBytes = heapU8.slice(responsePtr, responsePtr + responseLen);
      freeResponseFn(responsePtr);

      let parsed;
      try {
        parsed = JSON.parse(decoder.decode(responseBytes));
      } catch (error) {
        throw new WorkerProtocolError(
          'WASM_RESPONSE_INVALID',
          'WASM response is not valid JSON',
          { raw: decoder.decode(responseBytes), cause: String(error) },
          'pdftools_wasm_op'
        );
      }

      if (isPlainObject(parsed)) {
        if (parsed.ok === false && 'error' in parsed) {
          const payload = toErrorPayload(parsed.error, 'WASM_OP_FAILED');
          throw new WorkerProtocolError(payload.code, payload.message, payload.details, payload.context);
        }
        if (isPlainObject(parsed.status) && parsed.status.ok === false) {
          const payload = statusToErrorPayload(parsed.status, 'WASM_OP_FAILED');
          throw new WorkerProtocolError(payload.code, payload.message, payload.details, payload.context);
        }
      }

      return parsed;
    } finally {
      free(requestPtr);
      free(responsePtrPtr);
      free(responseLenPtr);
    }
  };
}

export async function createPdftoolsWasmBackend(initPayload = {}) {
  const moduleUrl = initPayload.moduleUrl;
  if (!moduleUrl || typeof moduleUrl !== 'string') {
    throw new WorkerProtocolError('WASM_MODULE_URL_REQUIRED', 'init.payload.moduleUrl is required for wasm backend');
  }

  let moduleInstance = null;
  let invokeWasm = null;

  return {
    name: 'pdftools-wasm',

    async init() {
      const imported = await import(moduleUrl);
      const moduleFactory = normalizeModuleFactory(imported);
      const factoryOptions = isPlainObject(initPayload.moduleOptions) ? initPayload.moduleOptions : {};
      moduleInstance = await moduleFactory(factoryOptions);
      invokeWasm = createWasmInvoker(moduleInstance);
      return {
        backend: 'wasm',
        moduleUrl
      };
    },

    async run(operationPayload = {}) {
      if (!invokeWasm) {
        throw new WorkerProtocolError('NOT_INITIALIZED', 'WASM backend is not initialized');
      }

      if (!isPlainObject(operationPayload)) {
        throw new WorkerProtocolError('BAD_REQUEST', 'run payload must be an object');
      }

      if (!isHostFsBridgePayload(operationPayload)) {
        return invokeWasm(operationPayload);
      }

      const bridgeTask = operationPayload.task;
      if (!isPlainObject(bridgeTask)) {
        throw new WorkerProtocolError('BAD_REQUEST', 'Bridge payload requires object field: task');
      }

      const fsApi = ensureFsApi(moduleInstance);
      stageBridgeInputs(fsApi, operationPayload.inputFiles);
      const wasmResult = invokeWasm(bridgeTask);
      const outputFiles = collectBridgeOutputs(fsApi, operationPayload.outputFiles);
      return {
        bridge: true,
        wasmResult,
        outputFiles
      };
    },

    async dispose() {
      if (moduleInstance && typeof moduleInstance.destroy === 'function') {
        moduleInstance.destroy();
      }
      moduleInstance = null;
      invokeWasm = null;
    }
  };
}

export async function createMockPdftoolsBackend(initPayload = {}) {
  const defaultLatencyMs = Number(initPayload.defaultLatencyMs ?? 0);
  let closed = false;

  return {
    name: 'mock',

    async init() {
      return {
        backend: 'mock',
        defaultLatencyMs: Number.isFinite(defaultLatencyMs) ? defaultLatencyMs : 0
      };
    },

    async run(operationPayload = {}) {
      if (closed) {
        throw new WorkerProtocolError('BACKEND_CLOSED', 'Mock backend has been disposed');
      }

      if (!isPlainObject(operationPayload)) {
        throw new WorkerProtocolError('BAD_REQUEST', 'run payload must be an object');
      }

      const latencyMsRaw = Number(operationPayload.latencyMs ?? defaultLatencyMs);
      const latencyMs = Number.isFinite(latencyMsRaw) && latencyMsRaw > 0 ? latencyMsRaw : 0;
      if (latencyMs > 0) {
        await sleep(latencyMs);
      }

      const opType = operationPayload.type;
      if (opType === 'fail') {
        throw new WorkerProtocolError(
          operationPayload.code || 'MOCK_FAIL',
          operationPayload.message || 'Mock failure from wasm worker',
          operationPayload.details || null
        );
      }

      if (opType === 'echo') {
        return {
          echoed: operationPayload.payload ?? null
        };
      }

      if (opType === 'sleep') {
        const ms = Number(operationPayload.ms ?? 0);
        if (Number.isFinite(ms) && ms > 0) {
          await sleep(ms);
        }
        return { sleptMs: Number.isFinite(ms) && ms > 0 ? ms : 0 };
      }

      return {
        ok: true,
        backend: 'mock',
        operation: operationPayload
      };
    },

    async dispose() {
      closed = true;
    }
  };
}
