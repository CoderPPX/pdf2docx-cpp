import test from 'node:test';
import assert from 'node:assert/strict';

import { createPdftoolsWasmBackend } from '../wasm/pdftools_wasm_backend.mjs';

const moduleUrl = new URL('./fixtures/mock_emscripten_module.mjs', import.meta.url).toString();
const heapViewModuleUrl = new URL(
  './fixtures/mock_emscripten_heap_view_module.mjs',
  import.meta.url
).toString();
const growthModuleUrl = new URL(
  './fixtures/mock_emscripten_memory_growth_module.mjs',
  import.meta.url
).toString();

test('wasm backend: init + run success', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });

  const initInfo = await backend.init();
  assert.equal(initInfo.backend, 'wasm');

  const result = await backend.run({
    type: 'echo',
    payload: { value: 42 }
  });

  assert.equal(result.ok, true);
  assert.deepEqual(result.payload, { echoed: { value: 42 } });

  await backend.dispose();
});

test('wasm backend: error envelope converts to WorkerProtocolError', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });
  await backend.init();

  await assert.rejects(
    async () => {
      await backend.run({ type: 'force-error' });
    },
    (error) => {
      assert.equal(error.code, 'INVALID_ARGUMENT');
      assert.equal(error.context, 'mock-wasm');
      assert.match(error.message, /forced envelope error/i);
      return true;
    }
  );

  await backend.dispose();
});

test('wasm backend: status envelope normalizes native-style code', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });
  await backend.init();

  await assert.rejects(
    async () => {
      await backend.run({ type: 'status-error' });
    },
    (error) => {
      assert.equal(error.code, 'NOT_FOUND');
      assert.equal(error.context, 'mock-status');
      assert.match(error.message, /missing input/i);
      return true;
    }
  );

  await backend.dispose();
});

test('wasm backend: non-zero op status maps to WASM_OP_FAILED', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });
  await backend.init();

  await assert.rejects(
    async () => {
      await backend.run({ type: 'return-non-zero' });
    },
    (error) => {
      assert.equal(error.code, 'WASM_OP_FAILED');
      assert.match(error.message, /non-zero status/i);
      return true;
    }
  );

  await backend.dispose();
});

test('wasm backend: invalid JSON response maps to WASM_RESPONSE_INVALID', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });
  await backend.init();

  await assert.rejects(
    async () => {
      await backend.run({ type: 'bad-json' });
    },
    (error) => {
      assert.equal(error.code, 'WASM_RESPONSE_INVALID');
      assert.match(error.message, /not valid json/i);
      return true;
    }
  );

  await backend.dispose();
});

test('wasm backend: host fs bridge stages inputs and collects outputs', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });
  await backend.init();

  const runResult = await backend.run({
    __hostFsBridge: true,
    task: {
      type: 'bridge-copy',
      inputPdf: '/work/in/0.pdf',
      outputPdf: '/work/out/0.pdf'
    },
    inputFiles: [
      {
        path: '/work/in/0.pdf',
        data: Uint8Array.from([11, 22, 33, 44])
      }
    ],
    outputFiles: [
      {
        path: '/work/out/0.pdf',
        hostPath: '/tmp/out.pdf'
      }
    ]
  });

  assert.equal(runResult.bridge, true);
  assert.equal(runResult.wasmResult.ok, true);
  assert.equal(runResult.outputFiles.length, 1);
  assert.equal(runResult.outputFiles[0].hostPath, '/tmp/out.pdf');
  assert.deepEqual(Array.from(runResult.outputFiles[0].data), [11, 22, 33, 44]);

  await backend.dispose();
});

test('wasm backend: host fs bridge rejects output spec without path', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });
  await backend.init();

  await assert.rejects(
    async () => {
      await backend.run({
        __hostFsBridge: true,
        task: {
          type: 'echo',
          payload: { ok: true }
        },
        outputFiles: [{}]
      });
    },
    (error) => {
      assert.equal(error.code, 'BAD_REQUEST');
      assert.match(error.message, /requires path/i);
      return true;
    }
  );

  await backend.dispose();
});

test('wasm backend: host fs bridge maps missing wasm output file to IO_ERROR', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl });
  await backend.init();

  await assert.rejects(
    async () => {
      await backend.run({
        __hostFsBridge: true,
        task: {
          type: 'echo',
          payload: { ok: true }
        },
        outputFiles: [
          {
            path: '/work/out/missing.pdf',
            hostPath: '/tmp/out.pdf'
          }
        ]
      });
    },
    (error) => {
      assert.equal(error.code, 'IO_ERROR');
      assert.equal(error.context, 'wasm.fs.readFile');
      assert.match(error.message, /failed to read wasm output file/i);
      return true;
    }
  );

  await backend.dispose();
});

test('wasm backend: bridge output remains safe after transferable clone', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl: heapViewModuleUrl });
  await backend.init();

  const first = await backend.run({
    __hostFsBridge: true,
    task: {
      type: 'bridge-copy',
      inputPdf: '/work/in/0.pdf',
      outputPdf: '/work/out/0.pdf'
    },
    inputFiles: [
      {
        path: '/work/in/0.pdf',
        data: Uint8Array.from([7, 8, 9, 10])
      }
    ],
    outputFiles: [
      {
        path: '/work/out/0.pdf',
        hostPath: '/tmp/out.pdf'
      }
    ]
  });

  assert.equal(first.bridge, true);
  assert.equal(first.outputFiles.length, 1);
  assert.equal(first.outputFiles[0].data.byteLength, 4);

  // Simulate worker postMessage transfer. If output buffer aliases wasm HEAP,
  // this would detach HEAP and break all subsequent wasm calls.
  structuredClone(
    { payload: first },
    {
      transfer: [first.outputFiles[0].data.buffer]
    }
  );

  const second = await backend.run({
    type: 'echo',
    payload: { value: 123 }
  });

  assert.equal(second.ok, true);
  assert.deepEqual(second.payload, { echoed: { value: 123 } });

  await backend.dispose();
});

test('wasm backend: invoker handles HEAP view refresh after memory growth', async () => {
  const backend = await createPdftoolsWasmBackend({ moduleUrl: growthModuleUrl });
  await backend.init();

  const result = await backend.run({
    type: 'echo',
    payload: { hello: 'growth' }
  });

  assert.equal(result.ok, true);
  assert.deepEqual(result.payload, { echoed: { hello: 'growth' } });

  await backend.dispose();
});
