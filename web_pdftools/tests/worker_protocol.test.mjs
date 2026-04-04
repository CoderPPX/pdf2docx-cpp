import test from 'node:test';
import assert from 'node:assert/strict';

import {
  WorkerProtocolError,
  normalizeErrorCode,
  statusToErrorPayload,
  toErrorPayload
} from '../wasm/worker_protocol.mjs';

test('worker protocol: normalize native error code aliases', () => {
  assert.equal(normalizeErrorCode('kInvalidArgument'), 'INVALID_ARGUMENT');
  assert.equal(normalizeErrorCode('kPdfParseFailed'), 'PDF_PARSE_FAILED');
  assert.equal(normalizeErrorCode('CUSTOM_CODE'), 'CUSTOM_CODE');
});

test('worker protocol: statusToErrorPayload keeps message and context', () => {
  const payload = statusToErrorPayload({
    code: 'kIoError',
    message: 'failed to read file',
    context: '/tmp/sample.pdf'
  });

  assert.equal(payload.code, 'IO_ERROR');
  assert.equal(payload.message, 'failed to read file');
  assert.equal(payload.context, '/tmp/sample.pdf');
  assert.equal(payload.details, null);
});

test('worker protocol: WorkerProtocolError exports context', () => {
  const error = new WorkerProtocolError('kInternalError', 'boom', { phase: 'run' }, 'wasm.run');
  const payload = toErrorPayload(error);

  assert.equal(payload.code, 'INTERNAL_ERROR');
  assert.equal(payload.message, 'boom');
  assert.equal(payload.context, 'wasm.run');
  assert.deepEqual(payload.details, { phase: 'run' });
});
