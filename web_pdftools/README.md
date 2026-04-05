# web_pdftools

Electron Web 页面，调用 `cpp_pdftools` 实现：

1. PDF 编辑：`merge` / `page delete` / `page insert` / `page replace`
2. PDF -> DOCX：`convert pdf2docx`
3. PDF 预览：`pdf.js` canvas 渲染，支持多 Tab

## 1. 先决条件

1. Node.js 18+
2. 已编译 `cpp_pdftools` 二进制 `pdftools`

可选方式：
1. 默认自动探测：
   - `../cpp_pdftools/build/linux-release/pdftools`
   - `../cpp_pdftools/build/linux-debug/pdftools`
   - 系统 `PATH` 里的 `pdftools`
2. 手动指定：
   - `PDFTOOLS_BIN=/abs/path/to/pdftools npm start`

## 2. 安装与运行

```bash
cd web_pdftools
npm install
npm start
```

## 3. 页面交互

1. Electron 原生 `File` 菜单：
   - `Open PDF...`
   - `Close Current Tab`
   - `Close All Tabs`
   - `Settings -> Theme & Backend Settings`
2. 右侧主区域：
   - Tab 切换
   - 上一页/下一页
   - 缩放
3. 左侧工具区：
   - 所有输入都支持“从已打开 Tab 选择”
4. Backend：
   - `Native CLI Tools`：稳定可用，调用本地 `pdftools` 二进制
   - `WASM Worker (Preview)`：通过 worker 调用 wasm C ABI（当前为 preview，功能逐步接入）

## 4. 代码结构

```text
web_pdftools/
  main.js        # Electron main + IPC + pdftools spawn
  preload.js     # 安全桥接 API
  index.html     # 页面结构（侧栏 + 预览）
  styles.css     # 页面样式
  renderer.mjs   # pdf.js 渲染 + 交互
  backends/      # native-cli / wasm backend 抽象
  wasm/dist/     # wasm 编译产物（pdftools_wasm.mjs/.wasm）
```

## 5. 已做校验

1. `node --check main.js`
2. `node --check preload.js`
3. `node --check renderer.mjs`
4. `npm run lint:node`
5. `npm run test:unit`（wasm worker 单元测试）

说明：当前环境未启动 GUI 窗口做端到端点击测试；需要本地桌面环境执行 `npm start` 验证。

## 6. 构建 wasm preview 模块

`WASM Worker (Preview)` backend 默认加载：

1. `wasm/dist/pdftools_wasm.mjs`
2. `wasm/dist/pdftools_wasm.wasm`

可执行：

```bash
cd web_pdftools
npm run build:wasm-stub
```

该命令会调用 `../cpp_pdftools/scripts/build_wasm_stub.sh` 重新生成 wasm 产物。

可选真实链路冒烟：

```bash
cd web_pdftools
npm run smoke:wasm -- --op merge --input /abs/a.pdf --input /abs/b.pdf --output /tmp/out.pdf
npm run smoke:wasm -- --op pdf2docx --input /abs/a.pdf --output /tmp/out.docx --dump-ir /tmp/out.ir.json
```

## 7. WASM Worker（新增）

目录：

```text
web_pdftools/wasm/
  worker_protocol.mjs
  pdftools_wasm_backend.mjs
  worker_dispatcher.mjs
  pdftools.worker.mjs        # 浏览器/Electron renderer worker 入口
  pdftools.node_worker.mjs   # Node 单元测试 worker 入口
  pdftools_worker_client.mjs # 主线程 Promise client
```

说明：

1. Worker 协议支持 `init / run / ping / shutdown`。
2. wasm backend 约定导出 C ABI：
   - `pdftools_wasm_op`（或 `_pdftools_wasm_op`）
   - `pdftools_wasm_free`（或 `_pdftools_wasm_free`）
3. 当前 wasm C ABI 已接入 `merge / delete-page / insert-page / replace-page / pdf2docx`（preview）。
4. renderer 侧已接入 host-fs bridge：输入文件会写入 wasm FS，任务执行后输出再回写宿主文件（含 `pdf2docx` 的 `.docx` / 可选 IR JSON）。
5. 页面侧边栏提供 WASM bridge 指标（最近一次详情 + 近5次平均耗时 + 近5次成功率）。
6. 当 WASM 因能力缺失或运行异常失败时，会自动尝试回退 Native CLI（若可用，可在 Settings 中关闭）。
7. 当前单元测试覆盖 worker 协议、超时、错误映射、wasm backend envelope 解析与 host-fs bridge。
