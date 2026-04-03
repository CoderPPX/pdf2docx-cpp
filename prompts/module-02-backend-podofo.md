# 模块 02：PDF 后端（PoDoFo）详细设计

## 1) 模块定位

本模块负责把 PDF 源文档解析为内部 IR 输入（当前直接输出 `ir::Document`），不负责布局推断和 DOCX 语义映射。

主文件：
- `cpp_pdf2docx/src/backend/podofo/podofo_backend.hpp`
- `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`

---

## 2) 当前公开接口（真实代码）

```cpp
class PoDoFoBackend {
 public:
  Status Open(const std::string& pdf_path) const;
  Status ExtractToIr(const std::string& pdf_path,
                     const ConvertOptions& options,
                     ir::Document* out_document) const;
};
```

输入：
- `pdf_path`
- `ConvertOptions`（当前主要用 `extract_images`）

输出：
- `ir::Document`（每页 `spans` + `images`）

---

## 3) 数据流与时序（现状）

1. `Open(pdf_path)`：
   - 检查 path 非空
   - 检查文件存在
   - 检查头部 `%PDF-`
2. `ExtractToIr(...)`：
   - `PdfMemDocument.Load(path)`
   - 遍历 pages：
     1. 填页面尺寸
     2. 文本提取 `ExtractTextTo(entries, params)`
     3. 将 `PdfTextEntry` 映射为 `ir::TextSpan`
     4. 若 `options.extract_images`：
        - `PdfContentStreamReader` 遍历内容流
        - 跟踪图形状态矩阵
        - 命中 `DoXObject(Image)` 抽图写 `ir::ImageBlock`
3. 返回 `Status::Ok()` 或错误码

---

## 4) 文本提取设计（已实现）

使用：
- `PoDoFo::PdfTextExtractParams`
  - `TokenizeWords`
  - `ComputeBoundingBox`

映射规则：
- `entry.Text -> span.text`
- `entry.X/Y/Length -> span.x/y/length`
- `entry.BoundingBox` 存在时：
  - `span.has_bbox=true`
  - 映射到 `span.bbox`

备注：
- 当前保持 PoDoFo 提取顺序，不在后端做额外阅读顺序调整。

---

## 5) 图片提取设计（已实现）

## 5.1 核心思想
通过内容流读取器捕获图形状态与 XObject 调用，在 `DoXObject` 上识别图片并提取二进制。

## 5.2 图形状态追踪
维护：
- `current_matrix`
- `graphics_state_stack`：用于 `q/Q`
- `form_context_stack`：处理 `BeginFormXObject/EndFormXObject`

处理规则：
- `q`：push current matrix
- `Q`：pop 恢复
- `cm`：`current_matrix = current_matrix * cmMatrix`
- `BeginFormXObject`：
  - 保存进入前状态
  - 叠加 form matrix
- `EndFormXObject`：
  - 恢复进入前状态

## 5.3 图片识别与提取
命中条件：
- `content.GetType() == DoXObject`
- `xobject->GetType() == Image`

格式推断（当前支持）：
- `DCTDecode -> image/jpeg + jpg`
- `JPXDecode -> image/jp2 + jp2`

字节提取：
- `stream->GetCopySafe()` -> `std::vector<uint8_t>`

位置尺寸：
- 使用 `placement_matrix = current_matrix * xobject->GetMatrix()`
- 变换单位矩形四个角
- 取 AABB 作为 `x/y/width/height`

ID 规则：
- `p<page>_img<index>`

---

## 6) 坐标与单位约定

1. 单位：`pt`
2. 原点：PDF 坐标（左下）
3. 后端不负责转换到 HTML/Word 坐标系
4. 若变换后尺寸非法（<=0）：
   - fallback 到 `PdfImage::GetWidth/GetHeight` 的近似值

---

## 7) 错误处理策略

1. 参数错误：
   - 空路径、null 输出 -> `kInvalidArgument`
2. IO 错误：
   - 文件不存在/不可读 -> `kIoError`
3. PDF 解析异常：
   - PoDoFo 异常 -> `kPdfParseFailed`
4. 图片抽取失败：
   - 单图片跳过，不中断整页和整文档

---

## 8) 已知风险与改进方向

1. **滤镜覆盖不足**：仅 DCT/JPX
2. **颜色空间 warning**：`ICCBased` 仍会警告
3. **几何精度**：AABB 对旋转图片有偏差
4. **const_cast 读取 filters**：PoDoFo 接口 const 性不理想

---

## 9) 后续详细任务（给下一个 LLM）

## P1
1. 增加 `FlateDecode` 路径（优先可导出 PNG）。
2. 将 warning 计数汇总到 `ConvertStats.warning_count`。
3. 在 `pdf2ir` 中输出每页过滤统计（抽取成功/跳过/失败）。

## P2
1. 支持 inline image（BI/EI）抽取。
2. 保留旋转信息（四角点）给后续 DOCX anchored 布局。

---

## 10) 验收标准

1. `build/test.pdf` 维持可抽图（当前 7 页多图）。
2. `converter_test` 中 `stats.image_count > 0` 继续通过。
3. 新增格式支持后，不破坏现有测试稳定性（当前 `16` 项）。

---

## 11) 最新进展（2026-04-03）

已落地：
1. `ExtractToIr(...)` 支持输出 `ImageExtractionStats`（成功数、跳过数、跳过原因、warning）。
2. 图片导出新增 `FlateDecode -> PNG` 路径（RGBA 解码后封装 PNG）。
3. 单图 decode 异常不再中断整份文档，改为“单图跳过并计数”。
4. `ConvertStats` 已接入后端统计映射（`warning_count`/skip reason 细分）。

---

## 12) 增量进展（2026-04-03, 图片兼容回退增强）

已补充两层图片回退策略：
1. 过滤器未知时，先尝试对原始 stream 字节做魔数识别（JPEG/PNG/JP2/GIF/BMP）并原样写出。
   - 原始流优先使用 `GetCopy(true)`，失败时回落 `GetCopySafe()`。
2. 解码转 PNG 路径从仅 `RGBA` 扩展到：
   - `RGBA`
   - `RGB24 -> RGBA`
   - `Grayscale -> RGBA`

说明：
- 后续已继续补齐 `ICCBased` 色彩空间 fallback（PoDoFo 侧），当前 `image-text.pdf` 已实现 `skipped_images=0`。

---

## 13) 增量进展（2026-04-03, ICCBased 专项）

为解决 `image-text.pdf` 中 ICCBased 图片缺失，已补两层修复：
1. 在 `thirdparty/podofo` 的 `PdfColorSpaceFilterFactory::TryCreateFromObject` 中增加
   `PdfColorSpaceType::ICCBased` 分支：
   - 优先解析 `Alternate` 颜色空间；
   - 其次按 `N` 分量数映射 `DeviceGray/RGB/CMYK`；
   - 最后保守回落 `DeviceRGB`。
2. 在本项目后端增加异常几何 page 归一化，避免极端 CTM 导致图片坐标落到页面外。

验证结果（`build/image-text.pdf`）：
- `images=7`
- `skipped_images=0`
- `warning_count=7`（主要来自几何归一化警告）
