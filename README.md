# NewWord

**macOS 轻量文字处理**：写得顺、存得住、能导出 PDF；DOCX 能开能改，**不承诺**与 Microsoft Word 完全一致。

NewWord 的存在理由很简单：**现代办公套件越来越臃肿**——插件、云同步、遥测和 COM 基础设施把启动拖到几秒，而大多数人只是想把一篇文档快速写好。NewWord 刻意砍掉这些"外围税"：单文档模型、按需加载、无插件，追求"打开即写、写完即走"的轻快体验。**性能哲学：宁可后台慢慢把文档准备完整，也绝不让用户等待。**

---

## 亮点

- **轻快启动**：无插件、无云同步、懒加载——打开和新建文档毫秒级
- **自研分页布局引擎**：预览 / PDF / 活页页数与页缝由 `engine/` 决定，编辑器页码与导出页码一致
- **内置 DOCX 双向转换**：不依赖外部服务；Python 桥接（mammoth/html2docx）可用时自动增强，失败自动回退
- **可靠性**：每分钟自动备份、崩溃/异常退出后可恢复草稿
- **快**：两三百页纯文本中部编辑约 3ms/键（实测，见[性能](#性能实测)）

## 能做什么

- 多标签编辑：新建 / 打开 / 保存 / 另存为 / 关闭（未保存会提示）
- 基础格式：字体、字号、粗斜下划线、颜色、对齐、列表、标题
- 插入：图片（属性/环绕）、表格（增删行列、合并）、常用 LaTeX 公式
- 查找替换、**自动备份（每分钟）**、崩溃/异常退出后可恢复草稿、记住上次打开的文件
- 页面视图 + 纸张/页边距；**页内可编辑页眉页脚**；功能区可折叠/隐藏
- 新建文档使用可配置默认字体 / 纸张（默认页码「第 N 页 / 共 M 页」）
- **导出 PDF / 分页预览**：由 `LayoutEngine` 分页绘制（正文、颜色、图片绕排、表格）
- 打开 PDF（只读预览，不可编辑）
- DOCX 打开与保存（正文 + **页眉页脚/纸张** 元数据）

## 架构

```text
编辑：QTextDocument（主编辑缓冲）
        ↕ QTextAdapter（fromDocument / toDocument）
     DocumentModel
        ├→ LayoutEngine → 预览 / PDF / 活页页数与页缝
        ├→ DocxExporter → 内置 DOCX 保存
        └← DocxImporter ← 内置 DOCX 打开
```

**设计取舍**：编辑器内断行由 Qt 排版（成熟、快），分页与 PDF 由自研引擎决定（页码对齐、可控）。不是「以引擎为中心的所见即所得编辑器」，但**页数已与引擎对齐**（见 [`docs/pagination_eval.md`](docs/pagination_eval.md)）。这个取舍换来的是开发速度与稳定性，代价是编辑视图与 PDF 行边界可能有细微差别——边界在哪里，README 如实交代。

## 性能实测

`tools/bench_editor_perf.cpp` 与 `tools/bench_mixed_doc.cpp`（offscreen，真实组件路径）：

| 文档 | 打开 | 尾部逐键 | **中部逐键** | 表格内逐键 |
|---|---|---|---|---|
| 200k 字符（≈190 页）纯文本 | 225ms | 0.00ms | **3.2ms** | — |
| 200k 字符 + 200 图 + 200 表 + 1.5 万格式碎片 | 357ms | 2.1ms | **66.9ms** | 127ms |
| 800k 字符（≈759 页）纯文本 | 930ms | 0.03ms | **13.3ms** | — |

结论：**纯文本/格式整齐的文档，三四百页内中部编辑无感**；带大量图片表格与碎片化格式时，中部编辑开始可感知（QTextDocument 的 O(n) 失效范围所致）——这是编辑缓冲的架构边界，也是后续自研编辑缓冲（见 RWord）的动机。基准可在本机复现：

```bash
./build/NewWordBenchEditorPerf   # 纯文本：打开/逐键/分页
./build/NewWordBenchMixedDoc     # 混合：图片+表格+混排格式
```

## 已知限制（v0.5）

- 无跟踪修订、无完整批注回复线程
- 无文本框 / 环绕 / 浮动对象
- 无 Word 级样式集、母版、域自动更新
- DOCX **有损近似**，复杂文档可能丢格式
- **引擎**：覆盖正文 / 表 / 图分页与四周型绕排（预览/PDF）；分栏、脚注、复杂表跨页（单元格拆分）等未做或不完整
- **活页编辑**：打开/显示时先 Fast 出纸张，Precise 延后到下一事件循环并在后台线程计算（状态栏「正在分页…」）；打字时 Fast 防抖 + 增量 Adapter；**行内断行仍由 Qt 排版**，与 PDF 行边界可能仍有细微差别。图片默认单独成段以缩小差异；四周型绕排在活页区为近似显示
- **页眉页脚**：页内可编默认页眉/页脚；首页不同/奇偶页仍以对话框为准。DOCX 读写为纯文本 + PAGE 域
- 当前面向 **macOS**（拼写检查、PDF 预览依赖系统框架）

## DOCX 主路径

**导入**：优先 Python 桥接（mammoth）；失败则 **`DocxImporter` → DocumentModel → `toDocument`**。  
**导出**：优先桥接（html2docx）；失败则 **`QTextDocument` → DocumentModel → `DocxExporter`**。

| 情况 | 行为 |
|------|------|
| 已运行 `setup.sh`，桥接正常 | 用增强引擎读写 |
| 桥接失败 | 自动回退内置（导出走 Model），状态栏提示 |
| 未安装桥接 | 直接用内置 Model 导出 / 子集导入 |

启用增强：

```bash
./tools/docx_bridge/setup.sh
```

需要项目目录下 `.venv`，并安装 `tools/docx_bridge/requirements.txt` 中的依赖。

## 构建与测试

依赖：Qt 6.11+（Widgets + PrintSupport）、macOS、zlib。可选：Python 3 + venv（DOCX 增强）。

```bash
# 开发构建
cmake --preset qt6-macos-debug
cmake --build --preset qt6-macos-debug
open build/NewWord.app

# 单元测试（ctest：DOCX 往返、LayoutEngine 分页、活页页数 vs 引擎页数）
ctest --test-dir build --output-on-failure
```

CI（GitHub Actions）：macOS 上自动构建测试 target、跑 ctest 与编辑性能基准，见 [`.github/workflows/ci.yml`](.github/workflows/ci.yml)。

## 安装（发布用 Bundle）

1. 构建 Release 后得到 `NewWord.app`
2. 拖入「应用程序」
3. 若需更好的 DOCX：在源码树执行 `./tools/docx_bridge/setup.sh`，或按打包说明附带 venv（可选）

## 验收样例

打开 [`examples/demo.docx`](examples/demo.docx)：

1. 应能看到标题、正文、表格  
2. 改几个字 → 保存  
3. 导出 PDF / 分页预览，内容大致可读、页数由 LayoutEngine 决定  

（公式若样例中包含，可再试编辑；复杂 Word 特性不在保证范围。）

## 文档

- 分页评测：[`docs/pagination_eval.md`](docs/pagination_eval.md)（Δ活页=0；几何估算短样例 ±1 / 长文·图·多页表 ±3）
- 毕设第 4～5 章提纲：[`docs/thesis_ch4_ch5_outline.md`](docs/thesis_ch4_ch5_outline.md)

## 关于框

菜单 **帮助 → 关于 NewWord**：版本 **0.5**、Qt 编辑 + LayoutEngine 预览/打印、DOCX 路径状态。
