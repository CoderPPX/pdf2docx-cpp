# 模块 04：字体系统（FreeType）详细设计

## 1) 模块定位

本模块负责字体能力探测与后续字体度量扩展。
当前已实现“探测链路”，用于在转换前验证 FreeType 可用性。

相关文件：
- `cpp_pdf2docx/include/pdf2docx/font.hpp`
- `cpp_pdf2docx/src/font/freetype_probe.cpp`

---

## 2) 当前实现状态

## 2.1 已实现
1. `ProbeFreeType()` 接口
2. 在 `Converter::ExtractIrFromFile` 中按 `options.enable_font_fallback` 调用
3. 有独立测试：
   - `tests/unit/freetype_test.cpp`

## 2.2 当前作用
- 不是字体匹配器
- 不是度量引擎
- 是“构建依赖 + 运行时可用性”的探针

---

## 3) 当前接口与调用位置

接口：
```cpp
Status ProbeFreeType();
```

调用链：
- `Converter::ExtractIrFromFile`：
  - `options.enable_font_fallback == true` 时先 probe
  - probe 失败则返回错误，不进入后端提取

---

## 4) 推荐演进设计（下一阶段）

建议逐步演化为三层：
1. `FontProbe`（当前已有）
2. `FontResolver`（字体名 -> 文件路径）
3. `FontMetricsProvider`（glyph 度量）

建议接口：
```cpp
struct FontResolveRequest { std::string family; bool bold; bool italic; uint32_t codepoint; };
struct FontResolveResult { std::string resolved_family; std::string file_path; bool fallback_used; };
struct GlyphMetrics { double ascent_pt, descent_pt, advance_pt, line_gap_pt; };
```

---

## 5) 与其他模块的边界

1. 后端模块（PoDoFo）只抽取文本/图片，不负责字体替换
2. Pipeline 可消费字体 metrics 做行距/段落推断
3. DOCX 写出可消费字体 family/style 做 `w:rPr`

---

## 6) 配置设计建议（可落地）

新增文件建议：`config/fonts.json`

建议字段：
```json
{
  "font_dirs": ["./fonts", "C:/Windows/Fonts"],
  "fallback": {
    "latin": ["Arial", "Times New Roman"],
    "cjk": ["Microsoft YaHei", "Noto Sans CJK SC"]
  },
  "aliases": {
    "Helvetica": "Arial",
    "Times-Roman": "Times New Roman"
  }
}
```

---

## 7) 测试设计建议

## P1 测试
1. probe 成功/失败路径（mock 或条件编译）
2. 缺失 FreeType 时报错语义

## P2 测试（实现 resolver/metrics 后）
1. 字体别名映射
2. fallback 命中统计
3. 度量单位一致性（px/pt 转换）

---

## 8) 风险与注意点

1. Windows 字体命名与 Linux/macOS 差异大
2. CJK 字体体积大，扫描性能可能成为瓶颈
3. 字体缓存应避免全局可变状态，防止并发问题

---

## 9) 任务卡（给下一位 LLM）

## P1
1. 保持现有 probe 稳定，不影响当前测试（`16` 项）。
2. 给 `ConvertStats` 增加字体 fallback 计数字段（占位可先为 0）。

## P2
1. 引入 `FontResolver`（最小实现：alias + fallback）。
2. 增加 `fonts.json` 读取与默认策略。
3. 对接 pipeline（行高/段落合并）。

---

## 10) 最新进展（2026-04-03）

已落地：
1. `ProbeFreeType` 已支持输出 `FreeTypeProbeInfo`（版本号、模块能力）。
2. 新增最小 `FontResolver`：
   - alias 映射
   - fallback family
   - family->file_path 映射
3. 已新增并接入 `font_resolver_test`，与现有 `freetype_test` 一并通过。
