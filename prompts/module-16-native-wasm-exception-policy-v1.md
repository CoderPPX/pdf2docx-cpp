# Module 16: Native/WASM 统一异常处理规范与落地（v1）

## 1. 目标

为 `cpp_pdftools` + `web_pdftools/wasm` 建立同一套异常处理规范，满足：

1. native/wasm 双端行为一致（失败都可转成结构化错误）。
2. 不允许 C++ 异常跨越模块边界（尤其未来 wasm C ABI 边界）。
3. CLI/GUI/Worker 都能拿到统一的 `code/message/context/details`。

---

## 2. 统一规范（必须遵守）

### 2.1 C++ 层规范（native 与 wasm 共用）

1. 对外 API（`pdftools::...`）统一返回 `Status`，不向外抛异常。
2. 三方库调用边界（PoDoFo/minizip/freetype 等）必须使用统一守卫包装：
   - `GuardStatus(...)`
   - 将异常映射为 `Status{code,message,context}`
3. 错误码优先使用业务语义：
   - 参数问题：`kInvalidArgument`
   - 文件/目录问题：`kIoError` / `kNotFound` / `kAlreadyExists`
   - PDF 解析/加载问题：`kPdfParseFailed`
   - 其余未预期异常：`kInternalError`
4. `context` 必填策略：
   - 传入操作标识或关键路径（如 operation_id、input_pdf/output_pdf）。

### 2.2 WASM/Worker 层规范

1. Worker 统一错误结构：

```json
{
  "code": "...",
  "message": "...",
  "context": "... | null",
  "details": {"...": "..."} | null
}
```

2. JS 协议层统一错误码标准化：
   - 支持 C++ 风格别名（`kInvalidArgument`）映射到统一字符串（`INVALID_ARGUMENT`）。
3. wasm 后端返回若含以下失败结构，必须立即抛为 `WorkerProtocolError`：
   - `{ ok: false, error: ... }`
   - `{ status: { ok: false, code/message/context... } }`

### 2.3 不允许的行为

1. 业务函数内散落重复 `try/catch`，且不同模块同类错误返回不同 code。
2. 把异常文本直接裸透给上层但没有 `context`。
3. 让异常穿透到 CLI 主循环/GUI 任务线程/worker 主循环。

---

## 3. 错误码统一映射

C++ `ErrorCode` -> Worker 统一字符串：

1. `kOk` -> `OK`
2. `kInvalidArgument` -> `INVALID_ARGUMENT`
3. `kIoError` -> `IO_ERROR`
4. `kPdfParseFailed` -> `PDF_PARSE_FAILED`
5. `kUnsupportedFeature` -> `UNSUPPORTED_FEATURE`
6. `kInternalError` -> `INTERNAL_ERROR`
7. `kNotFound` -> `NOT_FOUND`
8. `kAlreadyExists` -> `ALREADY_EXISTS`
9. `kCancelled` -> `CANCELLED`

---

## 4. 本次代码落地

### 4.1 新增统一异常桥接模块

1. `cpp_pdftools/include/pdftools/error_handling.hpp`
   - `ErrorCodeToString(...)`
   - `ExceptionToStatus(...)`
   - `CurrentExceptionToStatus(...)`
   - `GuardStatus(...)`（模板守卫）
2. `cpp_pdftools/src/core/error_handling.cpp`
   - 标准异常与 `std::bad_alloc` 到 `Status` 的统一映射。
3. `cpp_pdftools/CMakeLists.txt`
   - 把 `src/core/error_handling.cpp` 加入 `pdftools_core`。

### 4.2 C++ 模块异常处理统一

已改造文件：

1. `cpp_pdftools/src/pdf/document_ops.cpp`
   - `Merge/GetInfo/Delete/Insert/Replace` 全部使用 `GuardStatus` 统一边界。
2. `cpp_pdftools/src/pdf/create_ops.cpp`
   - 外层统一 `GuardStatus`，保持单图失败可跳过语义。
3. `cpp_pdftools/src/pdf/extract_ops.cpp`
   - `ExtractText/ExtractAttachments` 改为统一守卫。
   - 保持 `best_effort` fallback 逻辑不变。
4. `cpp_pdftools/src/convert/pdf2docx/pdf2docx.cpp`
   - legacy 转换调用统一守卫，避免异常外泄。
5. `cpp_pdftools/src/runtime/command_registry.cpp`
   - handler 异常统一转 `kInternalError`，`context=operation_id`。
6. `cpp_pdftools/src/runtime/task_runner.cpp`
   - 任务执行异常统一兜底转 `Status`。
7. `cpp_pdftools/src/app/cli_app.cpp`
   - 顶层 `RunCli` 增加统一异常兜底，避免 CLI 崩溃。
8. `cpp_pdftools_gui/src/services/pdftools_service.cpp`
   - GUI 任务执行加顶层异常兜底，线程内异常可转可展示错误文本。

### 4.3 WASM Worker 错误协议统一

1. `web_pdftools/wasm/worker_protocol.mjs`
   - 新增 `NativeErrorCode`、`normalizeErrorCode(...)`。
   - `WorkerProtocolError` 支持 `context`。
   - `toErrorPayload(...)` 统一输出 `code/message/context/details`。
   - 新增 `statusToErrorPayload(...)` 处理 C++ style status。
2. `web_pdftools/wasm/pdftools_wasm_backend.mjs`
   - 解析 wasm JSON 响应时识别失败结构并抛标准错误。
3. `web_pdftools/wasm/pdftools_worker_client.mjs`
   - 把 worker 返回的 `context` 透传到客户端异常。

---

## 5. 新增测试

1. C++：`cpp_pdftools/tests/unit/m7_error_handling_test.cpp`
   - 覆盖 `GuardStatus` 正常/异常/`bad_alloc` 路径。
   - 覆盖 `ErrorCodeToString`。
   - 覆盖 `CommandRegistry` 中 handler 抛异常映射。
2. JS：`web_pdftools/tests/worker_protocol.test.mjs`
   - 覆盖错误码别名映射。
   - 覆盖 status->error payload。
   - 覆盖 `WorkerProtocolError` 的 context 透传。

并更新：

- `cpp_pdftools/CMakeLists.txt`（注册 `m7` 测试）
- `web_pdftools/package.json`（lint 增加新测试文件）

---

## 6. 本地验证

### 6.1 `cpp_pdftools`

1. `cmake --preset linux-debug`：通过
2. `cmake --build --preset linux-debug -j8`：通过
3. `ctest --preset linux-debug`：`8/8` 通过（含 `m7_error_handling_test`）

### 6.2 `web_pdftools`

1. `npm run lint:node`：通过
2. `npm run test:unit`：`11/11` 通过（含 `worker_protocol.test.mjs`）

### 6.3 `cpp_pdftools_gui`

1. `cmake --preset linux-debug`：通过
2. `cmake --build --preset linux-debug -j8`：通过
3. `ctest --preset linux-debug`：`12/12` 通过

---

## 7. 对 wasm 友好的后续约束（下一步）

当前已完成“异常语义统一”和“worker协议统一”。
下一阶段建议继续：

1. 在真正 wasm C ABI 导出层强制 `noexcept` + `Status JSON envelope`。
2. 约束 `-fno-exceptions` 构建路径（如启用）时仍保持 `Status` 语义一致。
3. 增加 wasm 端集成测试：C++ op 真实失败 -> worker/client 错误码一致性断言。

