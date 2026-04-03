# 模块 08：图片链路专项设计（PDF 图片 -> IR -> HTML/DOCX）

## 1) 模块定位

这是一个跨模块专项文档，专门描述“图片转换链路”，用于后续继续增强图片质量时快速定位。

涉及文件：
- 后端抽图：`src/backend/podofo/podofo_backend.cpp`
- IR 结构：`include/pdf2docx/ir.hpp`
- HTML 渲染：`src/core/ir_html.cpp`
- DOCX 写出：`src/docx/p0_writer.cpp`
- 测试：`tests/unit/ir_html_image_test.cpp`、`tests/unit/docx_image_test.cpp`

---

## 2) 端到端数据流

1. PoDoFo 内容流遍历命中图片 XObject
2. 抽取编码字节 + 位置尺寸 -> `ir::ImageBlock`
3. `ir::Document` 进入下游：
   - HTML：base64 内嵌 `<img>`
   - DOCX：写 media part + drawing rel

---

## 3) 当前支持矩阵

## 3.1 图片格式
- 已支持：
  - `DCTDecode`（JPEG）
  - `JPXDecode`（JP2）
- 未覆盖：
  - `FlateDecode` 直接可视导出
  - `JBIG2Decode` 等更复杂格式

## 3.2 坐标
- 后端：PDF 坐标系（左下）
- HTML：转换为左上原点
- DOCX：当前仅 inline 尺寸，未做绝对锚定

---

## 4) 关键算法细节（当前）

1. 图形状态：
   - `q/Q/cm` 跟踪 matrix
2. Form 递归：
   - `BeginFormXObject` 入栈
   - `EndFormXObject` 恢复
3. 图片几何：
   - 用单位矩形四角经 CTM 变换
   - 取 AABB 作为 bbox
4. 二进制：
   - `GetCopySafe()` 获取可写入字节

---

## 5) 已知问题

1. `ICCBased` 颜色空间 warning（不阻塞当前 JPEG）
2. 旋转图在 AABB 下存在视觉偏差
3. DOCX 中图片位置偏“流式”而非“页面定位”

---

## 6) 优先改进路线

## P1
1. `FlateDecode -> PNG` 导出支持
2. `ConvertStats` 增加图片跳过原因统计
3. `pdf2ir` 输出每页图片提取统计

## P2
1. DOCX anchored 定位（坐标驱动）
2. 保留图像四角点并输出到 IR（替代仅 AABB）
3. 对颜色空间兼容做更稳妥处理

---

## 7) 验收建议

1. 对同一 PDF，IR 图片计数 = DOCX media 文件数
2. HTML 预览中图片数量与 IR 一致
3. 新增格式支持后，现有图片测试全部通过
