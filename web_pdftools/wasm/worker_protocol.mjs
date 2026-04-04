export const WorkerCommand = Object.freeze({
  Init: 'init',
  Run: 'run',
  Ping: 'ping',
  Shutdown: 'shutdown'
});

export const WorkerEvent = Object.freeze({
  Ready: 'ready'
});

export const NativeErrorCode = Object.freeze({
  OK: 'OK',
  INVALID_ARGUMENT: 'INVALID_ARGUMENT',
  IO_ERROR: 'IO_ERROR',
  PDF_PARSE_FAILED: 'PDF_PARSE_FAILED',
  UNSUPPORTED_FEATURE: 'UNSUPPORTED_FEATURE',
  INTERNAL_ERROR: 'INTERNAL_ERROR',
  NOT_FOUND: 'NOT_FOUND',
  ALREADY_EXISTS: 'ALREADY_EXISTS',
  CANCELLED: 'CANCELLED'
});

const NATIVE_ERROR_CODE_ALIASES = Object.freeze({
  kOk: NativeErrorCode.OK,
  kInvalidArgument: NativeErrorCode.INVALID_ARGUMENT,
  kIoError: NativeErrorCode.IO_ERROR,
  kPdfParseFailed: NativeErrorCode.PDF_PARSE_FAILED,
  kUnsupportedFeature: NativeErrorCode.UNSUPPORTED_FEATURE,
  kInternalError: NativeErrorCode.INTERNAL_ERROR,
  kNotFound: NativeErrorCode.NOT_FOUND,
  kAlreadyExists: NativeErrorCode.ALREADY_EXISTS,
  kCancelled: NativeErrorCode.CANCELLED,
  OK: NativeErrorCode.OK,
  INVALID_ARGUMENT: NativeErrorCode.INVALID_ARGUMENT,
  IO_ERROR: NativeErrorCode.IO_ERROR,
  PDF_PARSE_FAILED: NativeErrorCode.PDF_PARSE_FAILED,
  UNSUPPORTED_FEATURE: NativeErrorCode.UNSUPPORTED_FEATURE,
  INTERNAL_ERROR: NativeErrorCode.INTERNAL_ERROR,
  NOT_FOUND: NativeErrorCode.NOT_FOUND,
  ALREADY_EXISTS: NativeErrorCode.ALREADY_EXISTS,
  CANCELLED: NativeErrorCode.CANCELLED
});

export function normalizeErrorCode(code, defaultCode = NativeErrorCode.INTERNAL_ERROR) {
  if (typeof code !== 'string' || code.length === 0) {
    return defaultCode;
  }
  return NATIVE_ERROR_CODE_ALIASES[code] || code;
}

export class WorkerProtocolError extends Error {
  constructor(code, message, details = null, context = null) {
    super(message);
    this.name = 'WorkerProtocolError';
    this.code = normalizeErrorCode(code, NativeErrorCode.INTERNAL_ERROR);
    this.details = details;
    this.context = typeof context === 'string' && context.length > 0 ? context : null;
  }
}

export function isPlainObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

export function toErrorPayload(error, defaultCode = 'INTERNAL_ERROR') {
  const normalizedDefault = normalizeErrorCode(defaultCode, NativeErrorCode.INTERNAL_ERROR);
  if (error instanceof WorkerProtocolError) {
    return {
      code: normalizeErrorCode(error.code, normalizedDefault),
      message: error.message,
      context: error.context,
      details: error.details ?? null
    };
  }

  if (error && typeof error === 'object') {
    const code = normalizeErrorCode(
      typeof error.code === 'string' && error.code ? error.code : normalizedDefault,
      normalizedDefault
    );
    const message = typeof error.message === 'string' && error.message ? error.message : 'Unknown worker error';
    const context = typeof error.context === 'string' && error.context ? error.context : null;
    const details = 'details' in error ? error.details : null;
    return { code, message, context, details };
  }

  return {
    code: normalizedDefault,
    message: typeof error === 'string' ? error : 'Unknown worker error',
    context: null,
    details: null
  };
}

export function statusToErrorPayload(status, defaultCode = NativeErrorCode.INTERNAL_ERROR) {
  const normalizedDefault = normalizeErrorCode(defaultCode, NativeErrorCode.INTERNAL_ERROR);
  if (!isPlainObject(status)) {
    return {
      code: normalizedDefault,
      message: 'WASM operation failed',
      context: null,
      details: null
    };
  }

  const code = normalizeErrorCode(status.code, normalizedDefault);
  const message =
    typeof status.message === 'string' && status.message.length > 0 ? status.message : 'WASM operation failed';
  const context = typeof status.context === 'string' && status.context.length > 0 ? status.context : null;
  return {
    code,
    message,
    context,
    details: isPlainObject(status.details) ? status.details : null
  };
}

export function ensureRequestShape(request) {
  if (!isPlainObject(request)) {
    throw new WorkerProtocolError('BAD_REQUEST', 'Worker request must be an object');
  }

  if (typeof request.id !== 'string' || request.id.length === 0) {
    throw new WorkerProtocolError('BAD_REQUEST', 'Worker request.id must be a non-empty string');
  }

  if (typeof request.command !== 'string' || request.command.length === 0) {
    throw new WorkerProtocolError('BAD_REQUEST', 'Worker request.command must be a non-empty string');
  }

  if ('payload' in request && !isPlainObject(request.payload)) {
    throw new WorkerProtocolError('BAD_REQUEST', 'Worker request.payload must be an object when provided');
  }

  return request;
}

export function makeSuccessResponse(id, payload) {
  return { id, ok: true, payload };
}

export function makeErrorResponse(id, error) {
  return {
    id,
    ok: false,
    error: toErrorPayload(error)
  };
}

export function makeEventMessage(event, payload = {}) {
  return {
    type: 'event',
    event,
    payload
  };
}
