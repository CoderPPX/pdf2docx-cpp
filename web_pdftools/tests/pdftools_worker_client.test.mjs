import test from 'node:test';
import assert from 'node:assert/strict';
import { Worker } from 'node:worker_threads';

import { PdftoolsWorkerClient } from '../wasm/pdftools_worker_client.mjs';

const nodeWorkerUrl = new URL('../wasm/pdftools.node_worker.mjs', import.meta.url);

function createMockWorker() {
  return new Worker(nodeWorkerUrl, {
    type: 'module',
    workerData: {
      backend: 'mock'
    }
  });
}

test('worker client: start + run echo', async (t) => {
  const client = new PdftoolsWorkerClient({
    workerFactory: createMockWorker,
    requestTimeoutMs: 2000
  });

  t.after(() => {
    client.terminate();
  });

  const initResult = await client.start({ defaultLatencyMs: 1 });
  assert.equal(initResult.initialized, true);

  const runResult = await client.run({ type: 'echo', payload: { pdf: 'sample.pdf' } });
  assert.deepEqual(runResult, { echoed: { pdf: 'sample.pdf' } });
});

test('worker client: worker error mapped to protocol error', async (t) => {
  const client = new PdftoolsWorkerClient({
    workerFactory: createMockWorker,
    requestTimeoutMs: 2000
  });

  t.after(() => {
    client.terminate();
  });

  await client.start();

  await assert.rejects(
    async () => {
      await client.run({
        type: 'fail',
        code: 'UNIT_FAIL',
        message: 'forced failure'
      });
    },
    (error) => {
      assert.equal(error.code, 'UNIT_FAIL');
      assert.match(error.message, /forced failure/);
      return true;
    }
  );
});

test('worker client: request timeout', async (t) => {
  const client = new PdftoolsWorkerClient({
    workerFactory: createMockWorker,
    requestTimeoutMs: 2000
  });

  t.after(() => {
    client.terminate();
  });

  await client.start();

  await assert.rejects(
    async () => {
      await client.run(
        {
          type: 'sleep',
          ms: 150,
          latencyMs: 0
        },
        { timeoutMs: 10 }
      );
    },
    (error) => {
      assert.equal(error.code, 'TIMEOUT');
      assert.match(error.message, /timed out/i);
      return true;
    }
  );
});
