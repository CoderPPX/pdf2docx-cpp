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
2. 右侧主区域：
   - Tab 切换
   - 上一页/下一页
   - 缩放
3. 左侧工具区：
   - 所有输入都支持“从已打开 Tab 选择”

## 4. 代码结构

```text
web_pdftools/
  main.js        # Electron main + IPC + pdftools spawn
  preload.js     # 安全桥接 API
  index.html     # 页面结构（侧栏 + 预览）
  styles.css     # 页面样式
  renderer.mjs   # pdf.js 渲染 + 交互
```

## 5. 已做校验

1. `node --check main.js`
2. `node --check preload.js`
3. `node --check renderer.mjs`
4. `npm run lint:node`

说明：当前环境未启动 GUI 窗口做端到端点击测试；需要本地桌面环境执行 `npm start` 验证。
