# Native/WASM 统一异常处理规范与当前实现交接（exception_convention）

更新时间：2026-04-04
适用范围：`cpp_pdftools`、`cpp_pdftools_gui`、`web_pdftools/wasm`

---

## 1. 目标与原则

本规范用于保证同一套代码可在 native 与 wasm 场景下稳定运行，避免异常跨边界导致崩溃或不可观测错误。

核心原则：

1. C++ 业务 API 不向外抛异常，统一返回 `Status`。
2. 三方库调用点（PoDoFo、minizip、freetype 等）必须做异常到 `Status` 的收敛。
3. 错误输出统一结构：`code/message/context/details`（wasm worker 协议层）。
4. CLI/GUI/Worker 入口都要有兜底，防止进程级异常泄漏。

---

## 2. 统一错误模型

### 2.1 C++ ErrorCode（`pdftools::ErrorCode`）

1. `kOk`
2. `kInvalidArgument`
3. `kIoError`
4. `kPdfParseFailed`
5. `kUnsupportedFeature`
6. `kInternalError`
7. `kNotFound`
8. `kAlreadyExists`
9. `kCancelled`

### 2.2 wasm/worker 对外统一字符串错误码

1. `OK`
2. `INVALID_ARGUMENT`
3. `IO_ERROR`
4. `PDF_PARSE_FAILED`
5. `UNSUPPORTED_FEATURE`
6. `INTERNAL_ERROR`
7. `NOT_FOUND`
8. `ALREADY_EXISTS`
9. `CANCELLED`

兼容别名（已实现）：`kInvalidArgument` 等 C++ 风格 code 会标准化为上述全大写风格。

---

## 3. 本次新增的异常基础设施（C++）

### 3.1 新增文件

1. `cpp_pdftools/include/pdftools/error_handling.hpp`
2. `cpp_pdftools/src/core/error_handling.cpp`

### 3.2 提供能力

1. `ErrorCodeToString(ErrorCode)`
   - 统一 code -> 字符串映射（后续 wasm C ABI 也可复用）。
2. `ExceptionToStatus(...)`
   - 将 `std::exception` 转 `Status`。
3. `CurrentExceptionToStatus(...)`
   - 当前异常统一映射；对 `std::bad_alloc` 特判为 `kInternalError + "out of memory"`。
4. `GuardStatus(...)`
   - 模板守卫：执行 callable，捕获所有异常并统一转 `Status`。

### 3.3 CMake 接入

- 在 `cpp_pdftools/CMakeLists.txt` 中将 `src/core/error_handling.cpp` 纳入 `pdftools_core`。

---

## 4. 已完成的代码统一改造

## 4.1 runtime 层

1. `cpp_pdftools/src/runtime/command_registry.cpp`
   - `Execute(...)` 调用 handler 处增加 `GuardStatus`。
   - handler 抛异常时返回 `kInternalError`，`context=operation_id`。

2. `cpp_pdftools/src/runtime/task_runner.cpp`
   - 任务执行路径增加 `GuardStatus`，避免任务线程因异常直接失控。

## 4.2 pdf ops 层

1. `cpp_pdftools/src/pdf/document_ops.cpp`
   - `MergePdf/GetPdfInfo/DeletePage/InsertPage/ReplacePage` 外层统一使用 `GuardStatus`。
   - 保持原业务逻辑与原错误语义，减少分散 catch。

2. `cpp_pdftools/src/pdf/create_ops.cpp`
   - 外层改为 `GuardStatus`。
   - 保留“单图解码失败计入 skipped、继续处理后续图片”的策略。

3. `cpp_pdftools/src/pdf/extract_ops.cpp`
   - `ExtractText`/`ExtractAttachments` 改为统一守卫。
   - 保留 `best_effort` 回退语义（`pdftotext` fallback）不变。
   - primary 失败且允许 best_effort 时才走 fallback，最终仍保持 `Status` 语义一致。

## 4.3 convert 层

1. `cpp_pdftools/src/convert/pdf2docx/pdf2docx.cpp`
   - `ConvertPdfToDocx(...)` 外层增加 `GuardStatus`。
   - 任何未预期异常统一映射为 `kInternalError`，并带输入路径 context。

## 4.4 CLI 入口层

1. `cpp_pdftools/src/app/cli_app.cpp`
   - `RunCli(...)` 外围加总兜底 `try/catch`（使用 `CurrentExceptionToStatus`）。
   - 防止 CLI 主流程出现未捕获异常导致进程崩溃。

## 4.5 GUI 服务层

1. `cpp_pdftools_gui/src/services/pdftools_service.cpp`
   - `Execute(...)` 外围增加异常兜底。
   - 异常时以 `ExecutionOutcome{success=false}` 返回给任务系统展示，不让 GUI 线程直接失败。

---

## 5. wasm worker 协议统一改造

### 5.1 修改文件

1. `web_pdftools/wasm/worker_protocol.mjs`
2. `web_pdftools/wasm/pdftools_wasm_backend.mjs`
3. `web_pdftools/wasm/pdftools_worker_client.mjs`

### 5.2 改造内容

1. `worker_protocol.mjs`
   - 新增 `NativeErrorCode` 常量集。
   - 新增 `normalizeErrorCode(...)`：统一别名与格式。
   - `WorkerProtocolError` 扩展 `context` 字段。
   - `toErrorPayload(...)` 统一输出 `code/message/context/details`。
   - 新增 `statusToErrorPayload(...)`，可直接消费 C++ 风格 status 对象。

2. `pdftools_wasm_backend.mjs`
   - 解析 wasm 响应时检测失败 envelope：
     - `{ ok:false, error:... }`
     - `{ status:{ ok:false, ... } }`
   - 自动转 `WorkerProtocolError` 抛出并标准化 code/context。

3. `pdftools_worker_client.mjs`
   - 客户端反序列化 worker 错误时，透传 `context` 字段。

---

## 6. 新增测试与验证结果

## 6.1 C++ 新增测试

1. 新增：`cpp_pdftools/tests/unit/m7_error_handling_test.cpp`

覆盖：
1. `GuardStatus` 正常返回。
2. `GuardStatus` 捕获 `std::runtime_error` 并映射。
3. `GuardStatus` 捕获 `std::bad_alloc` 并映射 `out of memory`。
4. `ErrorCodeToString` 映射正确。
5. `CommandRegistry` handler 抛异常时映射为 `kInternalError` + `context=operation_id`。

同时在 `cpp_pdftools/CMakeLists.txt` 注册该测试目标：`pdftools_m7_error_handling_test`。

## 6.2 JS 新增测试

1. 新增：`web_pdftools/tests/worker_protocol.test.mjs`

覆盖：
1. 错误码别名标准化。
2. status 对象转 error payload。
3. `WorkerProtocolError` 的 `context/details` 透传。

并更新 `web_pdftools/package.json`：`lint:node` 增加该测试文件语法检查。

## 6.3 本地实测结果

### `cpp_pdftools`

1. `cmake --preset linux-debug`：通过
2. `cmake --build --preset linux-debug -j8`：通过
3. `ctest --preset linux-debug`：8/8 通过

### `web_pdftools`

1. `npm run lint:node`：通过
2. `npm run test:unit`：11/11 通过

### `cpp_pdftools_gui`

1. `cmake --preset linux-debug`：通过
2. `cmake --build --preset linux-debug -j8`：通过
3. `ctest --preset linux-debug`：12/12 通过

---

## 7. 规范落地要求（给下一个 agent）

后续新增代码请遵守：

1. 所有对外导出 API 必须返回 `Status`（或 wasm 协议错误对象），禁止裸抛异常。
2. 任何三方库强依赖调用边界优先用 `GuardStatus` 包裹。
3. 失败必须带可定位 `context`（路径/operation_id/阶段名至少其一）。
4. wasm 协议层只允许返回标准错误结构，不要发散自定义字段命名。
5. 新增模块时必须补充：
   - C++ 异常映射测试（至少 1 个）
   - wasm 错误协议测试（若模块走 worker）

---

## 8. 下一步建议（未完成项）

以下建议尚未在本轮完成，可由下个 agent 继续：

1. 在真正 C++ wasm C ABI 导出层实现统一 envelope：
   - `extern "C"` + `noexcept`
   - 入参 request JSON，出参 response JSON（含 status/error 结构）
2. 增加 wasm 集成测试（真实 wasm module，不仅 mock backend）：
   - 触发 native 失败并核对 worker/client 错误码一致性。
3. 逐步将 legacy `cpp_pdf2docx` 侧异常策略对齐到同一套 helper（目前主要完成的是 `cpp_pdftools` 主干）。

