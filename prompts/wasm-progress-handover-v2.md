# WASM 进度交接（thirdparty 编译链）V2

- 更新时间：2026-04-04
- 适用范围：`/home/zhj/Projects/pdftools/thirdparty`
- 目标：给下一个 LLM 直接接手 wasm 第三方库构建与后续集成。

## 1. 当前状态总览

本轮已经从“仅 roadmap”推进到“可重复构建 + 已有 wasm 静态库产物”：

1. 已把 `libiconv` 编译为 wasm 静态库
2. 已把 `libxml2` 编译为 wasm（`ICONV=ON`、`THREADS=OFF`）
3. 已把 `podofo` 编译为 wasm 静态库（`OPENSSL=OFF`）
4. 已新增一键脚本：`thirdparty/build_wasm_all.sh`
5. 线程已按要求关闭：`-sUSE_PTHREADS=0`

## 2. 关键产物（当前存在）

1. `zlib`:
- `/home/zhj/Projects/pdftools/thirdparty/build_wasm/zlib/libz.a`

2. `freetype2`:
- `/home/zhj/Projects/pdftools/thirdparty/build_wasm/freetype2/libfreetype.a`

3. `libiconv`:
- `/home/zhj/Projects/pdftools/thirdparty/build_wasm/libiconv-install/lib/libiconv.a`

4. `libxml2(iconv)`:
- `/home/zhj/Projects/pdftools/thirdparty/build_wasm/libxml2-install-iconv/lib/libxml2.a`

5. `podofo(no openssl)`:
- `/home/zhj/Projects/pdftools/thirdparty/build_wasm/podofo_noopenssl_xml2_iconv/src/podofo/libpodofo.a`

## 3. 已新增的一键脚本

文件：`/home/zhj/Projects/pdftools/thirdparty/build_wasm_all.sh`

功能：
1. 自动 patch `libiconv/source/lib/aliases.h`（非 Windows 使用 `intptr_t`）
2. 按顺序构建：`zlib -> freetype2 -> libiconv -> libxml2 -> podofo`
3. 默认 `-sUSE_PTHREADS=0`
4. 支持 `--clean` 全量重编

用法：

```bash
cd /home/zhj/Projects/pdftools
./thirdparty/build_wasm_all.sh
# 或
./thirdparty/build_wasm_all.sh --clean
```

## 4. 关键源码改动（podofo）

> 目的：允许 `OPENSSL=OFF` 时仍能通过构建，并在运行时明确报错“该能力不可用”。

已改：

1. `/home/zhj/Projects/pdftools/thirdparty/podofo/CMakeLists.txt`
- 增加 `PODOFO_HAVE_OPENSSL` 开关
- Emscripten 默认关闭 OpenSSL
- OpenSSL 依赖改为可选链接

2. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/CMakeLists.txt`
- `OPENSSL=OFF` 时替换加密/签名实现文件为 no-op 版本

3. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/private/CMakeLists.txt`
- `OPENSSL=OFF` 时替换私有 `CmsContext` 相关实现

4. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/auxiliary/podofo_config.h.in`
- 暴露 `PODOFO_HAVE_OPENSSL` 宏

5. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/main/PdfCommon.cpp`
- OpenSSL 初始化逻辑改为 `#if defined(PODOFO_HAVE_OPENSSL)` 条件编译

6. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/private/PdfWriter.cpp`
- 文件 ID 哈希不再强依赖 OpenSSL MD5
- OpenSSL 关闭时使用 deterministic fallback hash

7. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/private/PdfDeclarationsPrivate.cpp`
- Emscripten 下 `float/double` 格式化从 `to_chars` 切换到 `snprintf` fallback

新增文件：

1. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/main/PdfEncryptNoOpenSSL.cpp`
2. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/main/PdfSignerCmsNoOpenSSL.cpp`
3. `/home/zhj/Projects/pdftools/thirdparty/podofo/src/podofo/private/CmsContextNoOpenSSL.cpp`

行为：检测到加密/解密/CMS 签名场景时直接抛错，不提供伪能力。

## 5. libiconv 兼容性修补

文件：`/home/zhj/Projects/pdftools/thirdparty/libiconv/source/lib/aliases.h`

原因：原 gperf 代码在非 Windows 使用 `__int32`，Emscripten/clang 下不存在。

处理：
1. `_WIN64 -> __int64`
2. `_WIN32 -> __int32`
3. 其他平台改 `#include <stdint.h>` + `INT_PTR intptr_t`

## 6. 已验证结果

1. 脚本可完整跑通（2026-04-04 已执行）。
2. 配置确认：
- `libxml2`: `LIBXML2_WITH_ICONV=ON`, `LIBXML2_WITH_THREADS=OFF`
- `podofo`: `PODOFO_HAVE_OPENSSL=OFF`, `CMAKE_C_FLAGS/CMAKE_CXX_FLAGS=-sUSE_PTHREADS=0`
3. 符号确认：
- `libxml2.a` 需要 `libiconv/libiconv_open/libiconv_close`
- `libiconv.a` 提供对应符号

## 7. 已知注意事项

1. `OPENSSL=OFF` 后，以下能力故意不可用并会报错：
- 加密 PDF
- 解密受保护 PDF
- CMS 签名

2. 链接时不要把 `libiconv.a` 和 `libcharset.a` 都作为 whole-archive 强拉入，
会出现 `locale_charset` 重复符号。

3. `thirdparty/podofo` 里存在额外修改文件：
- `src/podofo/main/PdfColorSpaceFilter.cpp`
这部分不是本轮 wasm 开关核心改动，接手时请单独评估其来源与影响。

4. 当前输出是 `.a` 静态库，还不是最终浏览器可加载的 `wasm/js` 模块。

## 8. 未完成 / 下一步

1. 产出统一 C ABI（面向 Web 调用）并链接成最终 `*.wasm + *.js`。
2. 在 Web Worker 中封装调用协议（避免阻塞主线程）。
3. 明确 API 的输入输出（bytes in/out，错误码，日志）。
4. 增加 wasm 侧 test case（尤其 merge/delete/insert/replace/pdf2docx 冒烟）。
5. 评估是否继续保留 `libxml2` 或改用轻量 XML 方案（若仅用于 docx XML 组装）。

## 9. 当前 Git 状态提醒（交接风险）

根仓库 `git status` 显示：
1. `thirdparty/build_wasm_all.sh` 未跟踪
2. `thirdparty/libiconv/`、`thirdparty/libxml2/` 未跟踪
3. `thirdparty/podofo` 子模块有未提交修改

接手建议：
1. 先决定是否把这些改动提交到专门分支
2. 再做下一阶段 wasm API 集成，避免后续状态混杂
