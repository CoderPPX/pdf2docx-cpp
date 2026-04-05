import test from 'node:test';
import assert from 'node:assert/strict';
import { Worker } from 'node:worker_threads';

import { PdftoolsWorkerClient } from '../wasm/pdftools_worker_client.mjs';

const nodeWorkerUrl = new URL('../wasm/pdftools.node_worker.mjs', import.meta.url);
const moduleUrl = new URL('./fixtures/mock_emscripten_module.mjs', import.meta.url).toString();
const growthModuleUrl = new URL(
  './fixtures/mock_emscripten_memory_growth_module.mjs',
  import.meta.url
).toString();

function createWasmWorker() {
  return new Worker(nodeWorkerUrl, {
    type: 'module',
    workerData: {
      backend: 'wasm'
    }
  });
}

test('worker wasm integration: start + run success', async (t) => {
  const client = new PdftoolsWorkerClient({
    workerFactory: createWasmWorker,
    requestTimeoutMs: 3000
  });

  t.after(() => {
    client.terminate();
  });

  const initResult = await client.start({ moduleUrl });
  assert.equal(initResult.initialized, true);
  assert.equal(initResult.backend, 'pdftools-wasm');

  const runResult = await client.run({
    type: 'echo',
    payload: {
      pdf: 'a.pdf'
    }
  });

  assert.equal(runResult.ok, true);
  assert.deepEqual(runResult.payload, { echoed: { pdf: 'a.pdf' } });
});

test('worker wasm integration: status envelope error is normalized', async (t) => {
  const client = new PdftoolsWorkerClient({
    workerFactory: createWasmWorker,
    requestTimeoutMs: 3000
  });

  t.after(() => {
    client.terminate();
  });

  await client.start({ moduleUrl });

  await assert.rejects(
    async () => {
      await client.run({ type: 'status-error' });
    },
    (error) => {
      assert.equal(error.code, 'NOT_FOUND');
      assert.equal(error.context, 'mock-status');
      assert.match(error.message, /missing input/i);
      return true;
    }
  );
});

test('worker wasm integration: handles wasm memory growth without detached buffer errors', async (t) => {
  const client = new PdftoolsWorkerClient({
    workerFactory: createWasmWorker,
    requestTimeoutMs: 3000
  });

  t.after(() => {
    client.terminate();
  });

  await client.start({ moduleUrl: growthModuleUrl });

  const runResult = await client.run({
    type: 'echo',
    payload: { growth: true }
  });

  assert.equal(runResult.ok, true);
  assert.deepEqual(runResult.payload, { echoed: { growth: true } });
});
