# Web 侧异常规范对齐：下一步行动计划（v1）

更新时间：2026-04-04  
输入依据：`../prompts/exception_convention.md`

## 1. 现状结论

1. C++ 侧已完成 `GuardStatus` + `ErrorCode` 统一，CLI/GUI/runtime/pdf ops/convert 均已加兜底。  
2. `web_pdftools/wasm` 已具备错误标准化能力：`code/message/context/details`、别名归一（`kXxx -> UPPER_SNAKE_CASE`）。  
3. 当前 web 侧单测与语法检查已通过（11/11）。

## 2. 我接下来要做什么（按优先级）

## P0：先锁定 C++ WASM C ABI 契约并做前向兼容

目标：确保 native/wasm 错误语义一致，不因返回包体变化导致前端失配。

1. 与 C++ agent 对齐最终响应 envelope（建议固定）：
   - 成功：`{ ok: true, payload: ... }`
   - 失败：`{ ok: false, error: { code, message, context, details } }`
   - 兼容过渡：继续接受 `{ status: { ok:false, ... } }`（已有）
2. 在 `web_pdftools/wasm/pdftools_wasm_backend.mjs` 增加更严格校验：
   - 对 unknown/畸形响应返回 `WASM_RESPONSE_INVALID`
   - 在 `details` 带上原始响应片段，便于定位
3. 约束：任何错误都统一抛 `WorkerProtocolError`，禁止直接抛字符串/裸对象。

## P1：把错误上下文打通到 UI 日志

目标：用户看到的错误可直接定位到“哪个操作 + 哪个输入”。

1. 在 renderer 任务执行链追加结构化日志输出：
   - `error.code`
   - `error.message`
   - `error.context`
   - `error.details`（可选）
2. 将 `context` 约定用于显示输入文件/operation_id/阶段名，避免只显示“任务失败”。
3. 对常见 code 做轻量提示映射：
   - `INVALID_ARGUMENT` -> 参数问题
   - `IO_ERROR` -> 文件读写问题
   - `PDF_PARSE_FAILED` -> PDF 格式/内容异常
   - `UNSUPPORTED_FEATURE` -> 功能暂不支持

## P1：收敛 worker 协议边界（防止字段发散）

目标：worker/client 只交换标准错误结构，避免后续多 agent 改动引入不兼容字段。

1. 在 `worker_protocol.mjs` 明确：非标准字段进入 `details`，不新增顶层键。  
2. 在 `pdftools_worker_client.mjs` 对非标准错误包体统一转换为 `WorkerProtocolError(INTERNAL_ERROR)`，并保留原包到 `details.raw`。  
3. 在 `worker_dispatcher.mjs` 入口失败（shape/command）统一走 `makeErrorResponse`。

## P2：补齐测试矩阵（围绕异常规范）

目标：未来重构不回归。

1. 新增/扩展 JS 测试：
   - `pdftools_wasm_backend`：
     - `ok:false + error` 路径
     - `status.ok:false` 路径
     - 非法 JSON / 空响应 / 缺字段路径
   - `pdftools_worker_client`：
     - `context/details` 透传断言
     - worker runtime error / closed 场景
2. 预留 wasm 集成测试入口（等 C++ wasm C ABI 完成后启用）：
   - 真实 wasm 模块触发 native 失败
   - 校验最终 client 收到的 code 与 C++ `ErrorCode` 映射一致

## 3. 与另一位 C++ agent 的协作边界

1. C++ agent 负责：
   - `extern "C"` + `noexcept` 导出
   - 最终 JSON envelope 与错误字段稳定输出
2. 我负责：
   - web worker/backend/client 的兼容与校验
   - UI 层日志与错误可观测性
   - JS 侧测试和回归门禁

## 4. 完成定义（DoD）

1. JS 单测新增后全通过。  
2. 任意异常路径都能在 UI 日志看到 `code/message/context`。  
3. C++ wasm 包体变更后，web 端无需临时热修（通过兼容层吸收）。  
4. 不再出现未结构化错误（裸字符串、无 code 的错误对象）穿透到前端。

## 5. 执行顺序（我将按此顺序落地）

1. 先实现 P0（ABI 契约兼容 + backend 严格校验）  
2. 再做 P1（UI 结构化错误日志 + 协议收敛）  
3. 最后做 P2（测试补齐 + 集成测试脚手架）
