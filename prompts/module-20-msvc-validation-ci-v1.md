# Module 20: Linux 环境下验证 MSVC 编译运行链路（V1）

- 日期：2026-04-05
- 目标：在 Linux 开发环境中，为 `cpp_pdftools` 建立可执行的 MSVC 验证路径（通过 Windows CI 真机编译测试）。

## 1. 本轮改动

### 1.1 `cpp_pdftools` 增加 Windows/MSVC CMake 预设

文件：`cpp_pdftools/CMakePresets.json`

新增：
1. `configurePresets`
   - `windows-msvc-debug`
   - `windows-msvc-release`
2. `buildPresets`
   - `windows-msvc-debug`
   - `windows-msvc-release`
3. `testPresets`
   - `windows-msvc-debug`
   - `windows-msvc-release`

说明：
1. 生成器使用 `Visual Studio 17 2022`，架构 `x64`。
2. Debug/Release 通过 `configuration` 字段区分。

---

### 1.2 新增 `cpp_pdftools` 专属 CI（含 Windows MSVC）

文件：`.github/workflows/pdftools-ci.yml`

新增流水线：
1. `ubuntu-latest / linux-debug`
2. `windows-latest / windows-msvc-debug`

执行步骤：
1. `cmake --preset <preset>`
2. `cmake --build --preset <preset> --parallel 4`
3. `ctest --preset <preset>`

补充：
1. `actions/checkout` 开启 `submodules: recursive`，确保 `thirdparty` 可用。
2. 支持 `workflow_dispatch` 手动触发。

---

### 1.3 MSVC 兼容修复（代码层）

文件：`cpp_pdftools/src/pdf/extract_ops.cpp`

改动：
1. `TryExtractTextWithPdfToTextFallback(...)` 在 `_WIN32` 下直接返回 `kUnsupportedFeature`。
2. 避免 MSVC 环境编译/运行时走 `popen/pclose` 的 POSIX fallback 逻辑。

原因：
1. 该 fallback 依赖 `pdftotext` + POSIX shell 重定向，不适用于 Windows。
2. 目标是保证 Windows 构建稳定，通过统一错误返回保持行为可预期。

---

## 2. Linux 本地验证结果

在 Linux 侧已完成：
1. `cmake --list-presets=all` 可见新增 Windows preset。
2. `cpp_pdftools` 本地回归：
   - `cmake --build --preset linux-debug -j8` 通过
   - `ctest --preset linux-debug --output-on-failure` 通过（`8/8`）

说明：
1. Linux 不能本地执行 MSVC 编译，MSVC 结果需以 Windows CI 为准。

---

## 3. 你现在如何验证 MSVC

### 3.1 网页端
1. 推送分支到 GitHub。
2. 打开 `Actions` -> `cpp_pdftools_ci`。
3. 查看 `windows-latest / windows-msvc-debug` job：
   - Configure / Build / Test 三步均通过，即可判定 MSVC 编译运行通过。

### 3.2 命令行（可选）
```bash
gh workflow run pdftools-ci.yml --ref <your-branch>
gh run list --workflow pdftools-ci.yml --limit 1
gh run watch <run-id>
gh run view <run-id> --log-failed
```

---

## 4. 结论

`cpp_pdftools` 已具备“Linux 开发 + Windows/MSVC CI 验证”的完整链路。后续每次改动可通过同一工作流持续验证 MSVC 可编译/可测试状态。
