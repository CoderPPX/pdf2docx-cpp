import test from 'node:test';
import assert from 'node:assert/strict';

import {
  BackendId,
  WasmBackendMode,
  createTaskBackendRegistry,
  resolveBackendId
} from '../backends/task_backends.mjs';

function createMockApi() {
  return {
    async getStatus() {
      return {
        found: true,
        binaryPath: '/tmp/pdftools',
        hint: ''
      };
    },
    async runTask(task) {
      return {
        ok: true,
        code: 0,
        stdout: '',
        stderr: '',
        args: ['merge', '/tmp/out.pdf', ...(task?.inputPdfs || [])]
      };
    }
  };
}

test('task backends: resolveBackendId fallback', () => {
  assert.equal(resolveBackendId('native-cli'), BackendId.NativeCli);
  assert.equal(resolveBackendId('wasm'), BackendId.Wasm);
  assert.equal(resolveBackendId('unknown'), BackendId.NativeCli);
});

test('task backends: native cli backend status and run', async () => {
  const registry = createTaskBackendRegistry(createMockApi());
  const nativeBackend = registry.get(BackendId.NativeCli);
  assert.ok(nativeBackend);

  const status = await nativeBackend.getStatus();
  assert.equal(status.ready, true);
  assert.equal(status.code, 'OK');
  assert.match(status.message, /pdftools/);

  const runResult = await nativeBackend.runTask({
    type: 'merge',
    inputPdfs: ['/tmp/a.pdf', '/tmp/b.pdf'],
    outputPdf: '/tmp/out.pdf'
  });
  assert.equal(runResult.ok, true);
  assert.equal(runResult.backendId, BackendId.NativeCli);
});

test('task backends: wasm backend reserved mode', async () => {
  const registry = createTaskBackendRegistry(createMockApi(), {
    wasmMode: WasmBackendMode.Reserved
  });
  const wasmBackend = registry.get(BackendId.Wasm);
  assert.ok(wasmBackend);

  const status = await wasmBackend.getStatus();
  assert.equal(status.ready, false);
  assert.equal(status.code, 'BACKEND_RESERVED');

  const runResult = await wasmBackend.runTask({ type: 'pdf2docx' });
  assert.equal(runResult.ok, false);
  assert.equal(runResult.backendId, BackendId.Wasm);
  assert.match(runResult.stderr, /WASM backend/);
});

test('task backends: wasm worker mode status + run success', async () => {
  const fakeClient = {
    async start(payload) {
      assert.ok(payload?.moduleUrl);
      return { initialized: true };
    },
    async ping() {
      return { pong: true };
    },
    async run(task) {
      return { ok: true, echo: task.type };
    }
  };

  const registry = createTaskBackendRegistry(createMockApi(), {
    wasmMode: WasmBackendMode.Worker,
    wasmWorkerClientFactory: () => fakeClient,
    wasmModuleUrl: 'file:///tmp/pdftools_wasm.mjs'
  });
  const wasmBackend = registry.get(BackendId.Wasm);
  assert.ok(wasmBackend);

  const status = await wasmBackend.getStatus();
  assert.equal(status.ready, true);
  assert.equal(status.code, 'OK');

  const runResult = await wasmBackend.runTask({ type: 'merge' });
  assert.equal(runResult.ok, true);
  assert.equal(runResult.backendId, BackendId.Wasm);
  assert.match(runResult.stdout, /merge/);
});

test('task backends: wasm worker mode maps run error', async () => {
  const fakeError = new Error('unsupported');
  fakeError.code = 'UNSUPPORTED_FEATURE';
  fakeError.context = 'pdftools_wasm_op';

  const fakeClient = {
    async start() {
      return { initialized: true };
    },
    async ping() {
      return { pong: true };
    },
    async run() {
      throw fakeError;
    }
  };

  const registry = createTaskBackendRegistry(createMockApi(), {
    wasmMode: WasmBackendMode.Worker,
    wasmWorkerClientFactory: () => fakeClient,
    wasmModuleUrl: 'file:///tmp/pdftools_wasm.mjs'
  });
  const wasmBackend = registry.get(BackendId.Wasm);
  assert.ok(wasmBackend);

  const runResult = await wasmBackend.runTask({ type: 'merge' });
  assert.equal(runResult.ok, false);
  assert.equal(runResult.code, 'UNSUPPORTED_FEATURE');
  assert.equal(runResult.context, 'pdftools_wasm_op');
});

test('task backends: wasm worker host fs bridge reads and writes files', async () => {
  const writes = [];
  const hostFiles = new Map([
    ['/tmp/a.pdf', Uint8Array.from([1, 2, 3])],
    ['/tmp/b.pdf', Uint8Array.from([4, 5])]
  ]);

  const api = {
    async getStatus() {
      return { found: true, binaryPath: '/tmp/pdftools' };
    },
    async runTask() {
      return { ok: true, code: 0, stdout: '', stderr: '' };
    },
    async readFile(filePath) {
      const value = hostFiles.get(filePath);
      if (!value) {
        throw new Error(`missing host file: ${filePath}`);
      }
      return value;
    },
    async writeFile(payload) {
      writes.push(payload);
      return { ok: true };
    }
  };

  const fakeClient = {
    async start() {
      return { initialized: true };
    },
    async ping() {
      return { pong: true };
    },
    async run(request) {
      assert.equal(request.__hostFsBridge, true);
      assert.equal(request.task.type, 'merge');
      assert.equal(request.task.inputPdfs.length, 2);
      assert.equal(request.outputFiles.length, 1);
      assert.equal(request.outputFiles[0].hostPath, '/tmp/out.pdf');
      return {
        bridge: true,
        wasmResult: { ok: true, payload: { operation: 'merge' } },
        outputFiles: [
          {
            path: request.outputFiles[0].path,
            hostPath: '/tmp/out.pdf',
            data: Uint8Array.from([9, 9, 9])
          }
        ]
      };
    }
  };

  const registry = createTaskBackendRegistry(api, {
    wasmMode: WasmBackendMode.Worker,
    wasmWorkerClientFactory: () => fakeClient,
    wasmModuleUrl: 'file:///tmp/pdftools_wasm.mjs'
  });
  const wasmBackend = registry.get(BackendId.Wasm);

  const result = await wasmBackend.runTask({
    type: 'merge',
    inputPdfs: ['/tmp/a.pdf', '/tmp/b.pdf'],
    outputPdf: '/tmp/out.pdf'
  });

  assert.equal(result.ok, true);
  assert.equal(writes.length, 1);
  assert.equal(writes[0].filePath, '/tmp/out.pdf');
  assert.deepEqual(Array.from(writes[0].data), [9, 9, 9]);
  assert.equal(result.details.bridge, true);
  assert.equal(result.details.ioFiles.input, 2);
  assert.equal(result.details.ioFiles.output, 1);
  assert.equal(result.details.ioBytes.input, 5);
  assert.equal(result.details.ioBytes.output, 3);
  assert.ok(result.details.timingsMs.total >= 0);
});

test('task backends: wasm worker host fs bridge supports pdf2docx', async () => {
  const writes = [];
  const api = {
    async getStatus() {
      return { found: true, binaryPath: '/tmp/pdftools' };
    },
    async runTask() {
      return { ok: true, code: 0, stdout: '', stderr: '' };
    },
    async readFile(filePath) {
      if (filePath === '/tmp/source.pdf') {
        return Uint8Array.from([1, 2, 3, 4, 5]);
      }
      throw new Error(`missing host file: ${filePath}`);
    },
    async writeFile(payload) {
      writes.push(payload);
      return { ok: true };
    }
  };

  const fakeClient = {
    async start() {
      return { initialized: true };
    },
    async ping() {
      return { pong: true };
    },
    async run(request) {
      assert.equal(request.__hostFsBridge, true);
      assert.equal(request.task.type, 'pdf2docx');
      assert.match(request.task.inputPdf, /^\/work\/in\/0\.pdf$/);
      assert.match(request.task.outputDocx, /^\/work\/out\/0\.docx$/);
      assert.equal(request.outputFiles.length, 2);
      return {
        bridge: true,
        wasmResult: { ok: true, payload: { operation: 'pdf2docx' } },
        outputFiles: [
          {
            path: request.outputFiles[0].path,
            hostPath: '/tmp/output.docx',
            data: Uint8Array.from([80, 75, 3, 4])
          },
          {
            path: request.outputFiles[1].path,
            hostPath: '/tmp/output.ir.json',
            data: Uint8Array.from([123, 125])
          }
        ]
      };
    }
  };

  const registry = createTaskBackendRegistry(api, {
    wasmMode: WasmBackendMode.Worker,
    wasmWorkerClientFactory: () => fakeClient,
    wasmModuleUrl: 'file:///tmp/pdftools_wasm.mjs'
  });
  const wasmBackend = registry.get(BackendId.Wasm);

  const result = await wasmBackend.runTask({
    type: 'pdf2docx',
    inputPdf: '/tmp/source.pdf',
    outputDocx: '/tmp/output.docx',
    dumpIrPath: '/tmp/output.ir.json'
  });

  assert.equal(result.ok, true);
  assert.equal(writes.length, 2);
  assert.equal(writes[0].filePath, '/tmp/output.docx');
  assert.equal(writes[1].filePath, '/tmp/output.ir.json');
  assert.equal(result.details.bridge, true);
  assert.equal(result.details.ioFiles.input, 1);
  assert.equal(result.details.ioFiles.output, 2);
  assert.equal(result.details.ioBytes.input, 5);
  assert.equal(result.details.ioBytes.output, 6);
});

test('task backends: wasm worker bridge input read failure maps to prepare_input stage', async () => {
  const api = {
    async getStatus() {
      return { found: true, binaryPath: '/tmp/pdftools' };
    },
    async runTask() {
      return { ok: true, code: 0, stdout: '', stderr: '' };
    },
    async readFile(filePath) {
      throw new Error(`read denied: ${filePath}`);
    },
    async writeFile() {
      return { ok: true };
    }
  };

  let workerRunCalled = false;
  const fakeClient = {
    async start() {
      return { initialized: true };
    },
    async ping() {
      return { pong: true };
    },
    async run() {
      workerRunCalled = true;
      return { ok: true };
    }
  };

  const registry = createTaskBackendRegistry(api, {
    wasmMode: WasmBackendMode.Worker,
    wasmWorkerClientFactory: () => fakeClient,
    wasmModuleUrl: 'file:///tmp/pdftools_wasm.mjs'
  });
  const wasmBackend = registry.get(BackendId.Wasm);

  const result = await wasmBackend.runTask({
    type: 'merge',
    inputPdfs: ['/tmp/a.pdf', '/tmp/b.pdf'],
    outputPdf: '/tmp/out.pdf'
  });

  assert.equal(workerRunCalled, false);
  assert.equal(result.ok, false);
  assert.equal(result.code, 'IO_ERROR');
  assert.equal(result.context, 'wasm.hostfs.readInput');
  assert.equal(result.details.stage, 'prepare_input');
});

test('task backends: wasm worker bridge output write failure maps to persist_output stage', async () => {
  const api = {
    async getStatus() {
      return { found: true, binaryPath: '/tmp/pdftools' };
    },
    async runTask() {
      return { ok: true, code: 0, stdout: '', stderr: '' };
    },
    async readFile(filePath) {
      if (filePath === '/tmp/a.pdf') {
        return Uint8Array.from([1, 2, 3]);
      }
      if (filePath === '/tmp/b.pdf') {
        return Uint8Array.from([4, 5]);
      }
      throw new Error(`missing host file: ${filePath}`);
    },
    async writeFile() {
      throw new Error('disk full');
    }
  };

  const fakeClient = {
    async start() {
      return { initialized: true };
    },
    async ping() {
      return { pong: true };
    },
    async run(request) {
      return {
        bridge: true,
        wasmResult: { ok: true, payload: { operation: 'merge' } },
        outputFiles: [
          {
            path: request.outputFiles[0].path,
            hostPath: '/tmp/out.pdf',
            data: Uint8Array.from([9, 9, 9])
          }
        ]
      };
    }
  };

  const registry = createTaskBackendRegistry(api, {
    wasmMode: WasmBackendMode.Worker,
    wasmWorkerClientFactory: () => fakeClient,
    wasmModuleUrl: 'file:///tmp/pdftools_wasm.mjs'
  });
  const wasmBackend = registry.get(BackendId.Wasm);

  const result = await wasmBackend.runTask({
    type: 'merge',
    inputPdfs: ['/tmp/a.pdf', '/tmp/b.pdf'],
    outputPdf: '/tmp/out.pdf'
  });

  assert.equal(result.ok, false);
  assert.equal(result.code, 'IO_ERROR');
  assert.equal(result.context, 'wasm.hostfs.writeOutput');
  assert.equal(result.details.stage, 'persist_output');
});
