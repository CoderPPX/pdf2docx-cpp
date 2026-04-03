# 模块 06：API 与 CLI（库入口、工具入口）详细设计

## 1) 模块定位

本模块提供统一调用面：
- C++ API（`Converter`）
- CLI 工具（`pdf2docx`、`ir2html`、`pdf2ir`）
- 统一的状态码与统计输出

核心文件：
- `cpp_pdf2docx/include/pdf2docx/converter.hpp`
- `cpp_pdf2docx/include/pdf2docx/types.hpp`
- `cpp_pdf2docx/include/pdf2docx/status.hpp`
- `cpp_pdf2docx/src/api/converter.cpp`
- `cpp_pdf2docx/tools/cli/main.cpp`
- `cpp_pdf2docx/tools/ir2html/main.cpp`
- `cpp_pdf2docx/tools/pdf2ir/main.cpp`

---

## 2) 当前 API（真实实现）

## 2.1 `ConvertOptions`
```cpp
struct ConvertOptions {
  bool preserve_page_breaks = true;
  bool extract_images = true;
  bool detect_tables = false;
  bool enable_font_fallback = true;
  uint32_t max_threads = 0;
};
```

## 2.2 `ConvertStats`
```cpp
struct ConvertStats {
  uint32_t page_count = 0;
  uint32_t image_count = 0;
  uint32_t warning_count = 0;
  double elapsed_ms = 0.0;
  std::string backend;
  std::string xml_backend;
};
```

## 2.3 `Converter`
```cpp
Status ExtractIrFromFile(const std::string& input_pdf,
                         const ConvertOptions& options,
                         ir::Document* document) const;

Status ConvertFile(const std::string& input_pdf,
                   const std::string& output_docx,
                   const ConvertOptions& options,
                   ConvertStats* stats) const;
```

---

## 3) 当前编排逻辑（`ConvertFile`）

顺序固定：
1. 参数校验
2. `ExtractIrFromFile`（含 FreeType probe + PoDoFo backend）
3. `pipeline.Execute()`（当前占位）
4. `P0Writer.WriteFromIr(...)`
5. 汇总 stats（page/image/backend/xml/elapsed）

---

## 4) CLI 工具设计（现状）

## 4.1 `pdf2docx`
用法：
```bash
pdf2docx <input.pdf> <output.docx>
```
输出：
- 成功打印 `backend/xml/elapsed_ms`
- 失败打印状态消息

## 4.2 `ir2html`
用法：
```bash
ir2html <input.pdf> <output.html> [--scale 1.25] [--hide-boxes]
```
行为：
- 先提取 IR
- 再写 HTML 预览（文本 + 图片）

## 4.3 `pdf2ir`
用法：
```bash
pdf2ir <input.pdf> <output.json>
```
行为：
- 提取 IR 到 JSON（页、spans、images、`data_size`）

---

## 5) 状态码与错误语义建议

当前已有 `Status + ErrorCode`，建议保证：
1. 参数错误：`kInvalidArgument`
2. 文件 IO：`kIoError`
3. PDF 解析：`kPdfParseFailed`
4. 功能缺失：`kUnsupportedFeature`

消息规范建议：
- `module: concise reason`（便于日志检索）

---

## 6) 模块边界

1. API 不应直接依赖具体 thirdparty 类型
2. CLI 只做参数解析与编排调用，不写核心算法
3. 工具输出（HTML/JSON）用于调试与回归，不作为最终稳定协议

---

## 7) 后续设计（建议）

## P1
1. `pdf2docx` CLI 增加参数：
   - `--no-images`
   - `--disable-font-fallback`
   - `--dump-ir <path>`
2. `ConvertStats` 增加：
   - `skipped_image_count`
   - `backend_warning_count`
3. 提供 `ConvertFileToIrAndDocx(...)` 复合接口，减少重复提取。

## P2
1. 支持从内存 bytes 转换（`ConvertMemory`）。
2. 抽象日志接口（可注入 callback）。
3. 增加可取消机制（cancel token）。

---

## 8) 测试建议

1. API 级：
   - 空输入参数
   - 不存在路径
   - 正常统计字段
2. CLI 级：
   - 参数缺失返回码
   - 输出文件生成断言
   - 错误信息关键字断言

---

## 9) 交接执行卡

1. 改 API 前先跑 `ctest --preset linux-debug` 基线。
2. 新增 CLI 参数后同步更新 `README.md`。
3. 保持 `tools/*` 仅依赖 public headers，避免 include `src/*` 私有实现。
