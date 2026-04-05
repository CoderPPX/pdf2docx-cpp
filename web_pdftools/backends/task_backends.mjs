import { createPdftoolsWorkerClient } from '../wasm/pdftools_worker_client.mjs';

export const BackendId = Object.freeze({
  NativeCli: 'native-cli',
  Wasm: 'wasm'
});

export const WasmBackendMode = Object.freeze({
  Worker: 'worker',
  Reserved: 'reserved'
});

const WASM_BRIDGE_TASK_TYPES = new Set([
  'merge',
  'delete-page',
  'insert-page',
  'replace-page',
  'pdf2docx'
]);

export function resolveBackendId(value, fallback = BackendId.NativeCli) {
  if (value === BackendId.NativeCli || value === BackendId.Wasm) {
    return value;
  }
  return fallback;
}

function normalizeWasmBackendMode(value) {
  if (value === WasmBackendMode.Reserved || value === WasmBackendMode.Worker) {
    return value;
  }
  return WasmBackendMode.Worker;
}

function nowMs() {
  if (typeof performance !== 'undefined' && typeof performance.now === 'function') {
    return performance.now();
  }
  return Date.now();
}

function createStructuredError(code, message, context, details = null) {
  return {
    code,
    message,
    context,
    details
  };
}

function mapWorkerError(error, backendId) {
  const code = typeof error?.code === 'string' && error.code ? error.code : 'WASM_RUN_FAILED';
  const message = typeof error?.message === 'string' && error.message ? error.message : 'WASM backend request failed';
  return {
    ok: false,
    code,
    stdout: '',
    stderr: message,
    backendId,
    args: [],
    context: typeof error?.context === 'string' ? error.context : null,
    details: error?.details ?? null
  };
}

function shouldResetWasmWorker(error) {
  const code = typeof error?.code === 'string' ? error.code : '';
  return (
    code === 'WORKER_CLOSED' ||
    code === 'WORKER_UNAVAILABLE' ||
    code === 'WORKER_RUNTIME_ERROR' ||
    code === 'NOT_INITIALIZED'
  );
}

function isNonEmptyString(value) {
  return typeof value === 'string' && value.trim().length > 0;
}

function normalizeExt(filePath, fallbackExt) {
  if (!isNonEmptyString(filePath)) {
    return fallbackExt;
  }
  const normalized = filePath.replace(/\\/g, '/');
  const slash = normalized.lastIndexOf('/');
  const dot = normalized.lastIndexOf('.');
  if (dot <= slash) {
    return fallbackExt;
  }
  const ext = normalized.slice(dot);
  return /^[.][a-z0-9]{1,8}$/i.test(ext) ? ext.toLowerCase() : fallbackExt;
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
  throw new Error('Unsupported binary payload');
}

function hasHostFileBridgeApi(api) {
  return typeof api?.readFile === 'function' && typeof api?.writeFile === 'function';
}

function mapHostPath(inputHostPath, kind, index, fallbackExt = '.pdf') {
  return `/work/${kind}/${index}${normalizeExt(inputHostPath, fallbackExt)}`;
}

async function readHostFileAsBytes(hostFileApi, hostPath) {
  const payload = await hostFileApi.readFile(hostPath);
  return toUint8Array(payload);
}

async function buildWasmBridgePayload(task, hostFileApi) {
  if (!task || typeof task !== 'object') {
    return task || {};
  }
  if (!WASM_BRIDGE_TASK_TYPES.has(task.type)) {
    return task;
  }
  if (!hasHostFileBridgeApi(hostFileApi)) {
    return task;
  }

  const inputFiles = [];
  const outputFiles = [];

  const addInputFile = async (hostPath, wasmPath) => {
    try {
      inputFiles.push({
        path: wasmPath,
        data: await readHostFileAsBytes(hostFileApi, hostPath)
      });
    } catch (error) {
      throw createStructuredError(
        'IO_ERROR',
        `Failed to read bridge input file: ${hostPath}`,
        'wasm.hostfs.readInput',
        {
          hostPath,
          wasmPath,
          cause: error?.message || String(error)
        }
      );
    }
  };

  if (task.type === 'merge') {
    if (!Array.isArray(task.inputPdfs) || task.inputPdfs.length < 2 || !isNonEmptyString(task.outputPdf)) {
      return task;
    }

    const inputPdfs = [];
    for (let i = 0; i < task.inputPdfs.length; i += 1) {
      const hostPath = task.inputPdfs[i];
      if (!isNonEmptyString(hostPath)) {
        return task;
      }
      const wasmPath = mapHostPath(hostPath, 'in', i, '.pdf');
      await addInputFile(hostPath, wasmPath);
      inputPdfs.push(wasmPath);
    }

    const outputWasmPath = mapHostPath(task.outputPdf, 'out', 0, '.pdf');
    outputFiles.push({ path: outputWasmPath, hostPath: task.outputPdf });
    return {
      __hostFsBridge: true,
      task: {
        type: 'merge',
        inputPdfs,
        outputPdf: outputWasmPath,
        overwrite: true
      },
      inputFiles,
      outputFiles
    };
  }

  if (task.type === 'delete-page') {
    if (!isNonEmptyString(task.inputPdf) || !isNonEmptyString(task.outputPdf) || !Number(task.page)) {
      return task;
    }

    const inputPdf = mapHostPath(task.inputPdf, 'in', 0, '.pdf');
    await addInputFile(task.inputPdf, inputPdf);
    const outputPdf = mapHostPath(task.outputPdf, 'out', 0, '.pdf');
    outputFiles.push({ path: outputPdf, hostPath: task.outputPdf });
    return {
      __hostFsBridge: true,
      task: {
        type: 'delete-page',
        inputPdf,
        outputPdf,
        page: Number(task.page),
        overwrite: true
      },
      inputFiles,
      outputFiles
    };
  }

  if (task.type === 'insert-page') {
    if (
      !isNonEmptyString(task.inputPdf) ||
      !isNonEmptyString(task.sourcePdf) ||
      !isNonEmptyString(task.outputPdf) ||
      !Number(task.at) ||
      !Number(task.sourcePage)
    ) {
      return task;
    }

    const inputPdf = mapHostPath(task.inputPdf, 'in', 0, '.pdf');
    const sourcePdf = mapHostPath(task.sourcePdf, 'in', 1, '.pdf');
    await addInputFile(task.inputPdf, inputPdf);
    await addInputFile(task.sourcePdf, sourcePdf);
    const outputPdf = mapHostPath(task.outputPdf, 'out', 0, '.pdf');
    outputFiles.push({ path: outputPdf, hostPath: task.outputPdf });
    return {
      __hostFsBridge: true,
      task: {
        type: 'insert-page',
        inputPdf,
        sourcePdf,
        outputPdf,
        at: Number(task.at),
        sourcePage: Number(task.sourcePage),
        overwrite: true
      },
      inputFiles,
      outputFiles
    };
  }

  if (task.type === 'replace-page') {
    if (
      !isNonEmptyString(task.inputPdf) ||
      !isNonEmptyString(task.sourcePdf) ||
      !isNonEmptyString(task.outputPdf) ||
      !Number(task.page) ||
      !Number(task.sourcePage)
    ) {
      return task;
    }

    const inputPdf = mapHostPath(task.inputPdf, 'in', 0, '.pdf');
    const sourcePdf = mapHostPath(task.sourcePdf, 'in', 1, '.pdf');
    await addInputFile(task.inputPdf, inputPdf);
    await addInputFile(task.sourcePdf, sourcePdf);
    const outputPdf = mapHostPath(task.outputPdf, 'out', 0, '.pdf');
    outputFiles.push({ path: outputPdf, hostPath: task.outputPdf });
    return {
      __hostFsBridge: true,
      task: {
        type: 'replace-page',
        inputPdf,
        sourcePdf,
        outputPdf,
        page: Number(task.page),
        sourcePage: Number(task.sourcePage),
        overwrite: true
      },
      inputFiles,
      outputFiles
    };
  }

  if (task.type === 'pdf2docx') {
    if (!isNonEmptyString(task.inputPdf) || !isNonEmptyString(task.outputDocx)) {
      return task;
    }

    const inputPdf = mapHostPath(task.inputPdf, 'in', 0, '.pdf');
    await addInputFile(task.inputPdf, inputPdf);
    const outputDocx = mapHostPath(task.outputDocx, 'out', 0, '.docx');
    outputFiles.push({ path: outputDocx, hostPath: task.outputDocx });

    let dumpIrPath = '';
    if (isNonEmptyString(task.dumpIrPath)) {
      dumpIrPath = mapHostPath(task.dumpIrPath, 'out', 1, '.json');
      outputFiles.push({ path: dumpIrPath, hostPath: task.dumpIrPath });
    }

    return {
      __hostFsBridge: true,
      task: {
        type: 'pdf2docx',
        inputPdf,
        outputDocx,
        dumpIrPath,
        noImages: Boolean(task.noImages),
        anchoredImages: Boolean(task.anchoredImages),
        overwrite: true
      },
      inputFiles,
      outputFiles
    };
  }

  return task;
}

class NativeCliTaskBackend {
  constructor(api) {
    this.id = BackendId.NativeCli;
    this.label = 'Native CLI Tools';
    this._api = api;
  }

  async getStatus() {
    const status = await this._api.getStatus();
    return {
      backendId: this.id,
      backendLabel: this.label,
      ready: Boolean(status?.found),
      code: status?.found ? 'OK' : 'BINARY_NOT_FOUND',
      message: status?.found
        ? `已找到 pdftools: ${status.binaryPath}`
        : status?.hint || '未找到 pdftools binary',
      details: status || null
    };
  }

  async runTask(task) {
    const result = await this._api.runTask(task);
    return {
      ...result,
      backendId: this.id
    };
  }
}

class WasmReservedTaskBackend {
  constructor() {
    this.id = BackendId.Wasm;
    this.label = 'WASM (Reserved)';
  }

  async getStatus() {
    return {
      backendId: this.id,
      backendLabel: this.label,
      ready: false,
      code: 'BACKEND_RESERVED',
      message: 'WASM backend 预留中，当前版本请使用 Native CLI backend。',
      details: {
        stage: 'reserved'
      }
    };
  }

  async runTask(task) {
    return {
      ok: false,
      code: 'BACKEND_RESERVED',
      stdout: '',
      stderr: `WASM backend 尚未完成实现，任务未执行: ${task?.type || 'unknown'}`,
      backendId: this.id,
      args: []
    };
  }
}

class WasmWorkerTaskBackend {
  constructor(options = {}) {
    this.id = BackendId.Wasm;
    this.label = 'WASM Worker (Preview)';
    this._requestTimeoutMs = Number(options.requestTimeoutMs ?? 45_000);
    this._moduleUrl =
      options.moduleUrl ||
      new URL('../wasm/dist/pdftools_wasm.mjs', import.meta.url).toString();
    this._workerClientFactory = options.workerClientFactory || createPdftoolsWorkerClient;
    this._workerClientOptions = options.workerClientOptions || {};
    this._hostFileApi = options.hostFileApi || null;
    this._hostFsBridgeEnabled = options.hostFsBridgeEnabled !== false;

    this._client = null;
    this._started = false;
    this._startPromise = null;
  }

  _ensureClient() {
    if (this._client) {
      return this._client;
    }
    this._client = this._workerClientFactory({
      requestTimeoutMs: this._requestTimeoutMs,
      ...this._workerClientOptions
    });
    return this._client;
  }

  async _ensureStarted() {
    if (this._started) {
      return;
    }
    if (this._startPromise) {
      await this._startPromise;
      return;
    }

    const client = this._ensureClient();
    this._startPromise = (async () => {
      await client.start({
        moduleUrl: this._moduleUrl
      });
      this._started = true;
    })();

    try {
      await this._startPromise;
    } finally {
      this._startPromise = null;
    }
  }

  async _prepareRunPayload(task) {
    const startedAt = nowMs();
    const fallbackPayload = task || {};
    if (!this._hostFsBridgeEnabled) {
      return {
        payload: fallbackPayload,
        bridge: false,
        inputFiles: 0,
        inputBytes: 0,
        prepareInputMs: 0
      };
    }

    const payload = await buildWasmBridgePayload(fallbackPayload, this._hostFileApi);
    const isBridge = Boolean(payload?.__hostFsBridge);
    let inputFiles = 0;
    let inputBytes = 0;

    if (isBridge && Array.isArray(payload.inputFiles)) {
      inputFiles = payload.inputFiles.length;
      for (const file of payload.inputFiles) {
        if (!file || !('data' in file)) {
          continue;
        }
        try {
          inputBytes += toUint8Array(file.data).byteLength;
        } catch (_error) {
          // ignore size calculation failures
        }
      }
    }

    return {
      payload,
      bridge: isBridge,
      inputFiles,
      inputBytes,
      prepareInputMs: Math.max(0, nowMs() - startedAt)
    };
  }

  async _persistBridgeOutputs(runResult) {
    const startedAt = nowMs();
    if (!runResult?.bridge) {
      return {
        outputFiles: 0,
        outputBytes: 0,
        persistOutputMs: 0
      };
    }

    if (!Array.isArray(runResult.outputFiles) || runResult.outputFiles.length === 0) {
      return {
        outputFiles: 0,
        outputBytes: 0,
        persistOutputMs: Math.max(0, nowMs() - startedAt)
      };
    }

    if (typeof this._hostFileApi?.writeFile !== 'function') {
      throw createStructuredError(
        'IO_ERROR',
        'Host file API is missing writeFile() for wasm bridge output',
        'wasm.hostfs.writeOutput'
      );
    }

    let writtenFiles = 0;
    let writtenBytes = 0;
    for (const outputFile of runResult.outputFiles) {
      if (!isNonEmptyString(outputFile?.hostPath)) {
        continue;
      }
      if (!('data' in (outputFile || {}))) {
        continue;
      }

      try {
        await this._hostFileApi.writeFile({
          filePath: outputFile.hostPath,
          data: outputFile.data
        });
      } catch (error) {
        throw createStructuredError(
          'IO_ERROR',
          `Failed to persist bridge output file: ${outputFile.hostPath}`,
          'wasm.hostfs.writeOutput',
          {
            hostPath: outputFile.hostPath,
            wasmPath: outputFile.path || null,
            cause: error?.message || String(error)
          }
        );
      }

      writtenFiles += 1;
      try {
        writtenBytes += toUint8Array(outputFile.data).byteLength;
      } catch (_error) {
        // ignore size calculation failures
      }
    }

    return {
      outputFiles: writtenFiles,
      outputBytes: writtenBytes,
      persistOutputMs: Math.max(0, nowMs() - startedAt)
    };
  }

  async getStatus() {
    try {
      await this._ensureStarted();
      const ping = await this._client.ping();
      return {
        backendId: this.id,
        backendLabel: this.label,
        ready: true,
        code: 'OK',
        message: 'WASM worker backend 已就绪（preview）',
        details: {
          moduleUrl: this._moduleUrl,
          ping
        }
      };
    } catch (error) {
      this._started = false;
      return {
        backendId: this.id,
        backendLabel: this.label,
        ready: false,
        code: typeof error?.code === 'string' && error.code ? error.code : 'WASM_INIT_FAILED',
        message:
          typeof error?.message === 'string' && error.message
            ? error.message
            : 'WASM worker backend 初始化失败',
        details: {
          moduleUrl: this._moduleUrl,
          context: typeof error?.context === 'string' ? error.context : null,
          details: error?.details ?? null
        }
      };
    }
  }

  async runTask(task) {
    const startedAt = nowMs();
    let stage = 'start';
    let prepared = null;
    let workerRunMs = 0;
    try {
      await this._ensureStarted();
      stage = 'prepare_input';
      prepared = await this._prepareRunPayload(task);

      stage = 'worker_run';
      const workerStartedAt = nowMs();
      const runResult = await this._client.run(prepared.payload, {
        timeoutMs: this._requestTimeoutMs
      });
      workerRunMs = Math.max(0, nowMs() - workerStartedAt);

      stage = 'persist_output';
      const persist = await this._persistBridgeOutputs(runResult);
      const stdoutPayload = runResult?.bridge ? runResult.wasmResult : runResult;
      const isBridge = Boolean(prepared?.bridge || runResult?.bridge);
      const details = isBridge
        ? {
            bridge: true,
            taskType: task?.type || null,
            ioFiles: {
              input: prepared?.inputFiles ?? 0,
              output: persist.outputFiles
            },
            ioBytes: {
              input: prepared?.inputBytes ?? 0,
              output: persist.outputBytes
            },
            timingsMs: {
              prepareInput: Number((prepared?.prepareInputMs ?? 0).toFixed(3)),
              workerRun: Number(workerRunMs.toFixed(3)),
              persistOutput: Number((persist.persistOutputMs ?? 0).toFixed(3)),
              total: Number((Math.max(0, nowMs() - startedAt)).toFixed(3))
            }
          }
        : null;

      return {
        ok: true,
        code: 0,
        stdout: JSON.stringify(stdoutPayload ?? {}),
        stderr: '',
        backendId: this.id,
        args: [],
        details
      };
    } catch (error) {
      if (shouldResetWasmWorker(error)) {
        this._started = false;
      }
      const mapped = mapWorkerError(error, this.id);
      const baseDetails =
        mapped.details && typeof mapped.details === 'object'
          ? { ...mapped.details }
          : {};

      if (stage === 'prepare_input') {
        mapped.context = mapped.context || 'wasm.hostfs.readInput';
      } else if (stage === 'persist_output') {
        mapped.context = mapped.context || 'wasm.hostfs.writeOutput';
      } else if (stage === 'worker_run' && prepared?.bridge) {
        mapped.context = mapped.context || 'wasm.bridge.workerRun';
      }

      mapped.details = {
        ...baseDetails,
        stage,
        taskType: task?.type || null,
        elapsedMs: Number((Math.max(0, nowMs() - startedAt)).toFixed(3))
      };
      return mapped;
    }
  }

  async dispose() {
    if (!this._client) {
      return;
    }
    try {
      if (typeof this._client.shutdown === 'function') {
        await this._client.shutdown();
      }
    } catch (_error) {
      // no-op
    }
    if (typeof this._client.terminate === 'function') {
      this._client.terminate();
    }
    this._client = null;
    this._started = false;
    this._startPromise = null;
  }
}

export function createTaskBackendRegistry(api, options = {}) {
  const registry = new Map();
  registry.set(BackendId.NativeCli, new NativeCliTaskBackend(api));

  const wasmMode = normalizeWasmBackendMode(options.wasmMode);
  if (wasmMode === WasmBackendMode.Reserved) {
    registry.set(BackendId.Wasm, new WasmReservedTaskBackend());
  } else {
    registry.set(
      BackendId.Wasm,
      new WasmWorkerTaskBackend({
        moduleUrl: options.wasmModuleUrl,
        requestTimeoutMs: options.wasmRequestTimeoutMs,
        workerClientFactory: options.wasmWorkerClientFactory,
        workerClientOptions: options.wasmWorkerClientOptions,
        hostFileApi: options.wasmHostFileApi || api,
        hostFsBridgeEnabled: options.wasmHostFsBridgeEnabled
      })
    );
  }

  return registry;
}
