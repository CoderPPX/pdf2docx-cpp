# `cpp_pdftools` 编译到 WASM 的 Roadmap + Test Cases（V1）

- 更新时间：`2026-04-04`
- 目标目录：`/home/zhj/Projects/pdftools/web_pdftools`
- 范围：Web 端通过 WASM 调用 `cpp_pdftools` 做 PDF 编辑（合并、删页、插页、替换页、提取图片）

---

## 1. 目标与边界

## 1.1 目标

1. 将 `cpp_pdftools` 的 PDF 编辑能力迁移为浏览器可调用的 WASM SDK。
2. 首批支持能力：
   - `merge`
   - `delete page`
   - `insert page`
   - `replace page`
   - `extract images`（需要在 `cpp_pdftools` 增加公开 API）
3. 输出交付物：
   - `pdftools_wasm.wasm`
   - `pdftools_wasm.mjs`
   - `@web-pdftools/wasm` TypeScript 封装

## 1.2 非目标（本期不做）

1. 在 WASM 里复刻 CLI 路径型 API（Web 端应改为内存 Buffer API）。
2. 高级 PDF 内容重排编辑（段落回流、复杂对象重写）。
3. `pdf2docx` 同期迁移（可作为后续阶段，先完成编辑核心）。

---

## 2. 当前代码现状与差距

1. 现有 `cpp_pdftools` 主要是路径输入/输出（`std::filesystem` + 文件写入）。
2. 编辑能力已具备：`merge/delete/insert/replace`。
3. `extract images` 在 legacy 代码里有提取逻辑，但未作为 `pdftools::pdf` 公共 API 暴露。
4. `extract_ops` 有 `popen("pdftotext")` fallback，这在浏览器 WASM 不可用，需要编译开关关闭。

结论：需要“接口内存化 + WASM 适配层 + 提图 API 补齐”。

---

## 3. 目标架构（WASM）

```text
Web App (React/Vue/...)
  -> PDF Worker (Web Worker)
    -> JS Wrapper (@web-pdftools/wasm)
      -> C ABI (extern "C")
        -> cpp_pdftools core (PoDoFo backend)
          -> pdftools_wasm.wasm
```

## 3.1 核心设计原则

1. Web 侧只传 `Uint8Array`，不暴露本地路径。
2. WASM 导出稳定 C ABI，JS 封装为 Promise API。
3. 重任务强制跑 Worker，避免主线程卡顿。
4. 所有输出通过内存返回（或 WASM FS 临时文件后读取再释放）。

---

## 4. 分阶段 Roadmap

## M0（1 周）：WASM 可编译性打底

交付：
1. 新增 preset：`wasm-debug` / `wasm-release`（`emcmake cmake`）。
2. 先编译最小目标（仅 `GetPdfInfo`）验证 PoDoFo + thirdparty 在 Emscripten 下可链接。
3. 增加最小 smoke test（Node + 浏览器各 1 个）。

关键配置建议：
1. `-sMODULARIZE=1 -sEXPORT_ES6=1`
2. `-sALLOW_MEMORY_GROWTH=1`
3. `-sENVIRONMENT=web,worker`
4. 保留异常：`-sDISABLE_EXCEPTION_CATCHING=0`（PoDoFo 异常路径依赖）

里程碑门槛：
1. 能成功加载 wasm module。
2. 对 1 个小 PDF 返回正确页数。

## M1（1~1.5 周）：核心 API 内存化（无路径）

交付：
1. 新增内存版请求/响应结构：
   - `MergePdfFromBytes`
   - `DeletePageFromBytes`
   - `InsertPageFromBytes`
   - `ReplacePageFromBytes`
2. 原路径 API 保留（native CLI 继续可用），通过 adapter 调内存核心或反向包装。
3. 去除/隔离 WASM 不可用逻辑（`popen` fallback）。

建议新增类型：
1. `ByteView { const uint8_t* data; size_t size; }`
2. `ByteBuffer { std::vector<uint8_t> data; }`

里程碑门槛：
1. 四个编辑操作均可在“纯内存输入/输出”下完成。
2. 与 native 行为一致（页数/错误码对齐）。

## M2（1 周）：WASM C ABI + JS SDK

交付：
1. `src/wasm/pdftools_wasm_capi.cpp`：导出 C 接口。
2. 统一错误模型：`code + message + context`。
3. JS/TS 包装：
   - `mergePdfs(inputs: Uint8Array[]): Promise<Uint8Array>`
   - `deletePage(input: Uint8Array, page: number): Promise<Uint8Array>`
   - `insertPage(...)`
   - `replacePage(...)`

建议 C ABI：
1. `pdftools_wasm_op(request_ptr, request_len, response_ptr*)`
2. `pdftools_wasm_free(ptr)`
3. 请求/响应使用 JSON（首版易联调）或 FlatBuffer（二期优化）。

里程碑门槛：
1. 前端可在 Worker 里调用并拿到二进制 PDF 输出。
2. 不出现内存泄漏（100 次循环调用通过）。

## M3（1 周）：补齐 `extract images` 公共能力

交付：
1. 在 `pdftools::pdf` 新增：
   - `ExtractImagesRequest`
   - `ExtractImagesResult`
   - `Status ExtractImages(...)`
2. 复用 `legacy_pdf2docx/backend/podofo` 中图片提取逻辑。
3. WASM 返回结构：`[{name, mime, bytes}]`（可选 zip 打包模式）。

里程碑门槛：
1. 含 JPEG/PNG/无损图像的 PDF 能稳定提取。
2. 无图 PDF 返回空数组，不报错。

## M4（1 周）：Web 集成与稳定性

交付：
1. Worker 任务协议：`run/cancel/progress/result/error`。
2. 大文件策略：分配前做大小预检，超限提前失败。
3. 增加超时与取消（软取消：任务态；硬取消：重建 worker）。

里程碑门槛：
1. 100MB 级 PDF 不导致页面假死。
2. 用户可取消并恢复后续任务。

## M5（0.5~1 周）：性能优化与发布

交付：
1. `-O3 -flto` 产线构建。
2. 可选 `wasm-opt -O3`。
3. 产物体积预算与监控（gzip 后大小阈值）。
4. CI 自动构建与 npm 包发布流程。

里程碑门槛：
1. 首次加载与热调用性能达到阈值。
2. 跨浏览器主流程通过（Chrome/Edge/Safari 最新稳定版）。

---

## 5. 建议目录与文件落点

```text
cpp_pdftools/
  src/wasm/
    pdftools_wasm_capi.cpp
  include/pdftools/wasm/
    capi.h
  tests/wasm/
    wasm_smoke_test.mjs
    wasm_edit_ops_test.mjs
    wasm_extract_images_test.mjs
  cmake/
    ToolchainEmscripten.cmake (optional)

web_pdftools/
  packages/wasm-sdk/
    src/index.ts
    src/worker.ts
    test/*.spec.ts
```

---

## 6. 风险与规避

1. 风险：PoDoFo 在 wasm 下部分特性不稳定。
   - 规避：M0 单独做可编译性验证，失败则先裁剪 feature set（仅编辑核心）。
2. 风险：路径型 API 与 Web 场景冲突。
   - 规避：M1 内存 API 先行，native API 作为适配层。
3. 风险：提图格式兼容性复杂。
   - 规避：先支持常见编码（JPEG/PNG/Flate），其他格式返回 `unsupported` 标记。
4. 风险：大 PDF 内存峰值高。
   - 规避：Worker 隔离 + 内存预算 + 超限快速失败。

---

## 7. Test Cases（可直接执行）

## 7.1 构建与加载（Build/Boot）

| ID | 场景 | 步骤 | 预期 |
|---|---|---|---|
| B001 | wasm-debug 编译 | `emcmake cmake` + `cmake --build` | 生成 `.wasm/.mjs` 成功 |
| B002 | wasm-release 编译 | release 参数构建 | 生成产物且无未解析符号 |
| B003 | Node 加载 | `import()` wasm 模块并初始化 | 初始化成功，返回版本信息 |
| B004 | Browser Worker 加载 | Worker 中初始化 wasm | 初始化成功，无主线程阻塞 |

## 7.2 合并（Merge）

| ID | 场景 | 输入 | 预期 |
|---|---|---|---|
| M001 | 2 文件合并 | 2 个 1 页 PDF | 输出 2 页 |
| M002 | 多文件合并 | 10 个小 PDF | 页数为总和 |
| M003 | 空输入数组 | `[]` | 返回 `kInvalidArgument` |
| M004 | 含损坏 PDF | 1 正常 + 1 损坏 | 返回 `kPdfParseFailed` |
| M005 | 大文件合并 | 2 个 200 页 PDF | 成功且无崩溃，时间落在阈值内 |

## 7.3 删除页（Delete Page）

| ID | 场景 | 输入 | 预期 |
|---|---|---|---|
| D001 | 删除首页 | 3 页 PDF, page=1 | 输出 2 页 |
| D002 | 删除中间页 | 5 页 PDF, page=3 | 输出 4 页 |
| D003 | 删除末页 | 3 页 PDF, page=3 | 输出 2 页 |
| D004 | 越界页码 | 3 页 PDF, page=4 | `kInvalidArgument` |
| D005 | 删除唯一页 | 1 页 PDF, page=1 | `kUnsupportedFeature` |

## 7.4 插入页（Insert Page）

| ID | 场景 | 输入 | 预期 |
|---|---|---|---|
| I001 | 头部插入 | target 3 页, at=1 | 输出 4 页 |
| I002 | 中间插入 | target 3 页, at=2 | 输出 4 页 |
| I003 | 尾部插入 | target 3 页, at=4 | 输出 4 页 |
| I004 | `at=0` | 非法位置 | `kInvalidArgument` |
| I005 | source 页越界 | source_page 超范围 | `kInvalidArgument` |

## 7.5 替换页（Replace Page）

| ID | 场景 | 输入 | 预期 |
|---|---|---|---|
| R001 | 替换首页 | target page=1 | 输出页数不变 |
| R002 | 替换中间页 | target page=2 | 输出页数不变 |
| R003 | 替换末页 | target page=n | 输出页数不变 |
| R004 | target 越界 | page>n | `kInvalidArgument` |
| R005 | source 越界 | source_page>m | `kInvalidArgument` |

## 7.6 提取图片（Extract Images）

| ID | 场景 | 输入 | 预期 |
|---|---|---|---|
| E001 | 单图 PDF | 1 张 JPEG | 返回 1 张，mime 正确 |
| E002 | 多图混合编码 | JPEG + PNG + Flate | 返回数量正确，bytes 非空 |
| E003 | 无图 PDF | 纯文本 PDF | 返回空数组，不报错 |
| E004 | 重复 XObject 引用 | 同一图多次使用 | 按设计去重或保留，结果一致可预测 |
| E005 | 损坏图流 | 含异常图对象 PDF | 返回 warning 或跳过，不崩溃 |

## 7.7 错误处理与健壮性

| ID | 场景 | 步骤 | 预期 |
|---|---|---|---|
| S001 | 非法 request JSON | 传入错误结构 | `kInvalidArgument` + message |
| S002 | 超大输入 | 超过内存预算 | 快速失败，返回明确错误 |
| S003 | 取消任务 | Worker 执行中取消 | 任务终止，后续任务可继续 |
| S004 | 连续 100 次调用 | 循环执行 merge/delete | 无崩溃、无明显泄漏 |
| S005 | 并发任务 | 同时发起 3 个任务 | 按队列策略执行，结果不串扰 |

## 7.8 性能基线（建议阈值）

| ID | 场景 | 指标 | 阈值（建议） |
|---|---|---|---|
| P001 | wasm 首次初始化 | 冷启动时延 | <= 1500ms（桌面 Chrome） |
| P002 | 50 页删除一页 | 执行耗时 | <= 1200ms |
| P003 | 2x100 页合并 | 执行耗时 | <= 5000ms |
| P004 | 20 图提取 | 执行耗时 | <= 3000ms |
| P005 | 峰值内存 | 100MB 输入 | 峰值 < 600MB（可配置） |

---

## 8. 验收标准（DoD）

1. Web Worker 中可稳定执行 `merge/delete/insert/replace/extract images`。
2. C ABI 与 TS SDK 文档齐全，错误码统一。
3. 上述测试用例中 P0 集合全通过：`B001~B004, M001, D001, I001, R001, E001, S001, S004`。
4. CI 可自动构建 wasm 并产出可下载 artifact。

---

## 9. 开工优先级（建议）

1. 先做 `M0 + M1`，确认技术路径不堵塞。
2. 再做 `M2`，让前端尽快联调。
3. 最后补 `M3` 的提图 API 与完整回归。
