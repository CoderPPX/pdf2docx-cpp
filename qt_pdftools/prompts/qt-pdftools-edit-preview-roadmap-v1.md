# Qt PDFTools 编辑操作 Preview Roadmap (v1)

## 目标
为 `merge/delete-page/insert-page/replace-page` 提供“执行前预览”能力：
- 不覆盖用户目标输出文件
- 生成临时 PDF 结果并自动在预览 Tab 打开
- 与正式执行共用 backend 逻辑，减少行为偏差

## 设计方案
1. UI 入口
- 合并页新增 `预览结果` 按钮
- 删除页新增 `预览` 按钮
- 插入页新增 `预览` 按钮
- 替换页新增 `预览` 按钮

2. 执行模型
- 任务主链路新增 `preview_only` 标志
- preview 任务自动生成临时输出路径：`/tmp/qt_pdftools_preview/preview_<op>_<timestamp>.pdf`
- preview 成功后自动打开到新 Tab，保留缩放/翻页/多 Tab 体验

3. 状态与日志
- 日志明确标记：`任务 #N 开始：<任务>[预览]`
- 成功文案改为：`预览已生成`
- 失败文案区分预览失败/执行失败

4. 并发与交互
- 运行中统一禁用执行按钮和预览按钮
- 任务完成后统一恢复按钮可用

## 已实现（本次完成）
- [x] 四个编辑页均增加 preview 按钮
- [x] `RunTask/StartTaskExecution` 增加 `preview_only` 管线
- [x] 临时 PDF 输出路径生成函数 `BuildPreviewOutputPath(...)`
- [x] 任务日志和状态栏支持预览态文案
- [x] 任务运行期按钮统一禁用/恢复
- [x] 编译通过并完成 offscreen 启动 smoke test

## 关键代码位置
- `qt_pdftools/include/qt_pdftools/app/main_window.hpp`
- `qt_pdftools/src/app/main_window.cpp`

## 下一步建议（v2）
- [ ] 预览 Tab 打上标签（如 `[Preview] merge`）并支持“一键关闭所有预览 Tab”
- [ ] 预览文件生命周期管理（关闭 Tab 后自动删除临时文件）
- [ ] 对于大文件增加进度反馈与取消机制
- [ ] 为 preview 流程补充 Qt 单元测试（参数校验/路径生成/状态文案）

## 手动测试用例
1. Merge Preview
- 输入 2 个 PDF，点击 `预览结果`
- 预期：日志显示预览任务开始/成功；新建 Tab 打开临时 PDF

2. Delete Preview
- 选择输入 PDF，设置删除页，点击 `预览`
- 预期：预览结果页数减少 1；不改动正式输出路径文件

3. Insert Preview
- 选择目标与来源 PDF，设置 `at` 和 `source-page`，点击 `预览`
- 预期：新 Tab 打开插入后的结果，页序正确

4. Replace Preview
- 选择目标与来源 PDF，设置 `page` 和 `source-page`，点击 `预览`
- 预期：目标页内容被替换，其它页保持不变

5. Busy State
- 触发任一长任务后尝试再点其它预览/执行
- 预期：按钮禁用，不出现并发执行
