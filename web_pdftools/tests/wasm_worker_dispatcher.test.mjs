import test from 'node:test';
import assert from 'node:assert/strict';

import { createWorkerDispatcher } from '../wasm/worker_dispatcher.mjs';
import { createMockPdftoolsBackend } from '../wasm/pdftools_wasm_backend.mjs';

async function dispatch(dispatcher, request) {
  const messages = [];
  await dispatcher.handleMessage(request, (response) => {
    messages.push(response);
  });
  assert.equal(messages.length, 1);
  return messages[0];
}

test('worker dispatcher: run before init returns NOT_INITIALIZED', async () => {
  const dispatcher = createWorkerDispatcher({ backendFactory: createMockPdftoolsBackend });

  const response = await dispatch(dispatcher, {
    id: 'run-before-init',
    command: 'run',
    payload: { type: 'echo', payload: { value: 1 } }
  });

  assert.equal(response.ok, false);
  assert.equal(response.error.code, 'NOT_INITIALIZED');
});

test('worker dispatcher: init and run succeed', async () => {
  const dispatcher = createWorkerDispatcher({ backendFactory: createMockPdftoolsBackend });

  const initResponse = await dispatch(dispatcher, {
    id: 'init-1',
    command: 'init',
    payload: { defaultLatencyMs: 0 }
  });

  assert.equal(initResponse.ok, true);
  assert.equal(initResponse.payload.initialized, true);
  assert.equal(initResponse.payload.reused, false);

  const runResponse = await dispatch(dispatcher, {
    id: 'run-1',
    command: 'run',
    payload: { type: 'echo', payload: { x: 42 } }
  });

  assert.equal(runResponse.ok, true);
  assert.deepEqual(runResponse.payload, { echoed: { x: 42 } });
});

test('worker dispatcher: init twice reuses backend', async () => {
  const dispatcher = createWorkerDispatcher({ backendFactory: createMockPdftoolsBackend });

  const first = await dispatch(dispatcher, {
    id: 'init-a',
    command: 'init',
    payload: {}
  });
  const second = await dispatch(dispatcher, {
    id: 'init-b',
    command: 'init',
    payload: {}
  });

  assert.equal(first.ok, true);
  assert.equal(second.ok, true);
  assert.equal(second.payload.reused, true);
});

test('worker dispatcher: shutdown resets state', async () => {
  const dispatcher = createWorkerDispatcher({ backendFactory: createMockPdftoolsBackend });

  await dispatch(dispatcher, {
    id: 'init-c',
    command: 'init',
    payload: {}
  });

  const shutdownResponse = await dispatch(dispatcher, {
    id: 'shutdown-c',
    command: 'shutdown',
    payload: {}
  });

  assert.equal(shutdownResponse.ok, true);
  assert.deepEqual(shutdownResponse.payload, { shutdown: true });

  const runAfterShutdown = await dispatch(dispatcher, {
    id: 'run-after-shutdown',
    command: 'run',
    payload: { type: 'echo', payload: { v: 3 } }
  });

  assert.equal(runAfterShutdown.ok, false);
  assert.equal(runAfterShutdown.error.code, 'NOT_INITIALIZED');
});

test('worker dispatcher: invalid request shape returns BAD_REQUEST', async () => {
  const dispatcher = createWorkerDispatcher({ backendFactory: createMockPdftoolsBackend });

  const response = await dispatch(dispatcher, {
    id: '',
    command: 'init',
    payload: {}
  });

  assert.equal(response.ok, false);
  assert.equal(response.error.code, 'BAD_REQUEST');
});
