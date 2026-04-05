const encoder = new TextEncoder();
const decoder = new TextDecoder();

function align4(value) {
  return (value + 3) & ~3;
}

function toUint8Array(data) {
  if (data instanceof Uint8Array) {
    return new Uint8Array(data);
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
  throw new Error('invalid fs payload');
}

function normalizePath(filePath) {
  if (typeof filePath !== 'string' || filePath.length === 0) {
    throw new Error('invalid path');
  }
  const normalized = filePath.replace(/\\/g, '/');
  return normalized.startsWith('/') ? normalized : `/${normalized}`;
}

export default async function createMockEmscriptenHeapViewModule() {
  const memory = new ArrayBuffer(1024 * 1024);
  const HEAPU8 = new Uint8Array(memory);
  const HEAPU32 = new Uint32Array(memory);
  const fsDirs = new Set(['/']);
  const fsFiles = new Map();
  let nextPtr = 16;

  function _malloc(size) {
    const bytes = align4(Math.max(1, Number(size) || 0));
    const ptr = nextPtr;
    nextPtr += bytes;
    if (nextPtr >= HEAPU8.length) {
      throw new Error('mock wasm out of memory');
    }
    return ptr;
  }

  function _free(_ptr) {
    // no-op
  }

  function writeResponseBytes(bytes, responsePtrPtr, responseLenPtr) {
    const responsePtr = _malloc(bytes.length);
    HEAPU8.set(bytes, responsePtr);
    HEAPU32[responsePtrPtr >>> 2] = responsePtr >>> 0;
    HEAPU32[responseLenPtr >>> 2] = bytes.length >>> 0;
  }

  const FS = {
    mkdir(dirPath) {
      fsDirs.add(normalizePath(dirPath));
    },
    analyzePath(targetPath) {
      const normalized = normalizePath(targetPath);
      return {
        exists: fsDirs.has(normalized) || fsFiles.has(normalized)
      };
    },
    writeFile(filePath, data) {
      fsFiles.set(normalizePath(filePath), toUint8Array(data));
    },
    readFile(filePath) {
      const normalized = normalizePath(filePath);
      const data = fsFiles.get(normalized);
      if (!data) {
        throw new Error(`missing file: ${normalized}`);
      }

      // Return a HEAP-backed view to mimic real-world FS implementations.
      // If caller transfers this buffer directly, wasm HEAP would detach.
      const offset = 128 * 1024;
      HEAPU8.set(data, offset);
      return HEAPU8.subarray(offset, offset + data.length);
    }
  };

  function _pdftools_wasm_op(requestPtr, requestLen, responsePtrPtr, responseLenPtr) {
    const requestBytes = HEAPU8.slice(requestPtr, requestPtr + requestLen);
    let request;
    try {
      request = JSON.parse(decoder.decode(requestBytes));
    } catch (_error) {
      return 9;
    }

    if (request?.type === 'bridge-copy') {
      const inputPdf = request?.inputPdf;
      const outputPdf = request?.outputPdf;
      if (typeof inputPdf !== 'string' || typeof outputPdf !== 'string') {
        writeResponseBytes(
          encoder.encode(
            JSON.stringify({
              ok: false,
              error: {
                code: 'INVALID_ARGUMENT',
                message: 'bridge-copy requires inputPdf/outputPdf'
              }
            })
          ),
          responsePtrPtr,
          responseLenPtr
        );
        return 0;
      }

      const inputBytes = FS.readFile(inputPdf);
      FS.writeFile(outputPdf, inputBytes);
      writeResponseBytes(
        encoder.encode(
          JSON.stringify({
            ok: true,
            payload: {
              operation: 'bridge-copy',
              bytes: inputBytes.length
            }
          })
        ),
        responsePtrPtr,
        responseLenPtr
      );
      return 0;
    }

    writeResponseBytes(
      encoder.encode(
        JSON.stringify({
          ok: true,
          payload: {
            echoed: request?.payload ?? null
          }
        })
      ),
      responsePtrPtr,
      responseLenPtr
    );
    return 0;
  }

  function _pdftools_wasm_free(_ptr) {
    // no-op
  }

  return {
    HEAPU8,
    HEAPU32,
    _malloc,
    _free,
    FS,
    _pdftools_wasm_op,
    _pdftools_wasm_free,
    destroy() {}
  };
}
