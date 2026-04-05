const encoder = new TextEncoder();
const decoder = new TextDecoder();

function align4(value) {
  return (value + 3) & ~3;
}

function detachBuffer(buffer) {
  // Transfer to force-detach, simulating wasm memory growth semantics.
  structuredClone({ buffer }, { transfer: [buffer] });
}

export default async function createMockEmscriptenMemoryGrowthModule() {
  let buffer = new ArrayBuffer(256 * 1024);
  let HEAPU8 = new Uint8Array(buffer);
  let HEAPU32 = new Uint32Array(buffer);
  let nextPtr = 16;

  const moduleObj = {
    HEAPU8,
    HEAPU32,
    _malloc,
    _free,
    _pdftools_wasm_op,
    _pdftools_wasm_free,
    destroy() {}
  };

  function syncHeaps(newBuffer) {
    buffer = newBuffer;
    HEAPU8 = new Uint8Array(buffer);
    HEAPU32 = new Uint32Array(buffer);
    moduleObj.HEAPU8 = HEAPU8;
    moduleObj.HEAPU32 = HEAPU32;
  }

  function growMemory(minExtra) {
    const oldBuffer = buffer;
    const oldBytes = new Uint8Array(oldBuffer);
    const targetSize = Math.max(oldBuffer.byteLength * 2, oldBuffer.byteLength + align4(minExtra + 4096));
    const newBuffer = new ArrayBuffer(targetSize);
    const newBytes = new Uint8Array(newBuffer);
    newBytes.set(oldBytes);
    syncHeaps(newBuffer);
    detachBuffer(oldBuffer);
  }

  function _malloc(size) {
    const bytes = align4(Math.max(1, Number(size) || 0));
    if (nextPtr + bytes >= HEAPU8.length) {
      growMemory(bytes);
    }
    const ptr = nextPtr;
    nextPtr += bytes;
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

  function _pdftools_wasm_op(requestPtr, requestLen, responsePtrPtr, responseLenPtr) {
    const requestBytes = HEAPU8.slice(requestPtr, requestPtr + requestLen);
    let request;
    try {
      request = JSON.parse(decoder.decode(requestBytes));
    } catch (_error) {
      return 9;
    }

    // Force growth/detach during op execution to exercise stale HEAP view bugs.
    growMemory(32 * 1024);

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

  return moduleObj;
}
