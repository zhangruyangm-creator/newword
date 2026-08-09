# NewWord

**macOS 轻量文字处理**：写得顺、存得住、能导出 PDF；DOCX 能开能改，**不承诺**与 Microsoft Word 完全一致。

NewWord 的存在理由很简单：**现代办公套件越来越臃肿**——插件、云同步、遥测和 COM 基础设施把启动拖到几秒，而大多数人只是想把一篇文档快速写好。NewWord 刻意砍掉这些"外围税"：单文档模型、按需加载、无插件，追求"打开即写、写完即走"的轻快体验。**性能哲学：宁可后台慢慢把文档准备完整，也绝不让用户等待。**

---

## 亮点

- **轻快启动**：无插件、无云同步、懒加载——打开和新建文档毫秒级
- **自绘分页编辑器**：编辑器按真实纸张逐页绘制，文字、页眉、页脚互不穿越；打字走增量重排（只排光标页附近），停止输入后补齐精确页数
- **自研分页引擎**：预览 / PDF / 导出的分页与绕排由 `engine/` 决定
- **内置 DOCX 双向转换**：不依赖外部服务；Python 桥接（mammoth/html2docx）可用时自动增强，失败自动回退
- **可靠性**：每分钟自动备份、崩溃/异常退出后可恢复草稿
- **快**：两三百页纯文本中部编辑约 1.6ms/键、80 万字约 7ms/键（实测，见[性能](#性能实测)）

## 能做什么

- 多标签编辑：新建 / 打开 / 保存 / 另存为 / 关闭（未保存会提示）
- 基础格式：字体、字号、粗斜下划线、颜色、对齐、列表、标题
- 插入：图片（属性/环绕）、表格（增删行列、合并、**列宽拖动调整**）、常用 LaTeX 公式
- 查找替换、**自动备份（每分钟）**、崩溃/异常退出后可恢复草稿、记住上次打开的文件
- 页面视图 + 纸张/页边距；页眉页脚（对话框编辑，分页编辑器内绘制）；功能区可折叠/隐藏
- 新建文档使用可配置默认字体 / 纸张（默认页码「第 N 页 / 共 M 页」）
- **导出 PDF / 分页预览**：由 `LayoutEngine` 分页绘制（正文、颜色、图片绕排、表格）
- 打开 PDF（只读预览，不可编辑）
- DOCX 打开与保存（正文 + **页眉页脚/纸张** 元数据）

## 架构

```text
编辑：PagedEditorWidget（自绘分页编辑器：逐页裁剪绘制、hitTest 定位、输入法/选区）
        └─ QTextDocument（分页页尺寸 = 页面内容框；编辑器分页以此为准）
     DocumentModel（↕ QTextAdapter fromDocument / toDocument）
        ├→ LayoutEngine → 预览 / PDF / 导出分页与绕排
        ├→ DocxExporter → 内置 DOCX 保存
        └← DocxImporter ← 内置 DOCX 打开
```

**设计取舍**：编辑器是一个自绘分页控件——QTextDocument 按「内容框宽 × 内容框高」真实分页，控件逐页裁剪绘制、用 `hitTest` 定位光标；打字走增量分页（只重排光标页附近，空闲 120ms 后补齐全文页数与滚动条）。预览 / PDF 仍由 `engine/` 的 LayoutEngine 绘制。两者的断行来自同一套 Qt 排版，行边界基本一致；编辑器页数以 QTextDocument 为准、导出以引擎为准，复杂文档仍可能有细微差别（见 [`docs/pagination_eval.md`](docs/pagination_eval.md)）。

## 性能实测

`tools/bench_editor_perf.cpp`（自绘编辑器，offscreen 真实组件路径）与 `tools/bench_mixed_doc.cpp`（编辑缓冲 QTextDocument）：

| 文档 | 打开 | 尾部逐键 | **中部逐键** | 表格内逐键 | 整页重绘 |
|---|---|---|---|---|---|
| 200k 字符（≈126 页）纯文本 | 145ms | 0.03ms | **1.6ms** | — | — |
| 200k 字符 + 200 图 + 200 表 + 1.5 万格式碎片 | 328ms | 2.1ms | **69ms** | 127ms | — |
| 800k 字符（≈500 页）纯文本 | 595ms | 0.07ms | **6.8ms** | — | — |
| 800k 字符 + 1112 张大图（≈1670 页） | 852ms | 0.20ms | **22ms** | — | **2.9ms** |

结论：**纯文本/格式整齐的文档，80 万字内中部编辑都在 60fps 预算内**；带大量表格与碎片化格式时，中部编辑开始可感知（QTextDocument 的 O(n) 失效范围所致）——这是编辑缓冲的架构边界，也是后续自研编辑缓冲（见 RWord）的动机。图片已做资源降采样与像素图缓存，整页重绘约 3ms。数据为 Debug 构建（offscreen），Release 会更快。基准可在本机复现：

```bash
./build/NewWordBenchEditorPerf   # 纯文本：打开/逐键/分页
./build/NewWordBenchMixedDoc     # 混合：图片+表格+混排格式
```

## 已知限制（v0.5）

- 无跟踪修订、无完整批注回复线程
- 有浮动文本框（覆盖层：插入/拖动/缩放/编辑文字），但**无文字环绕**（文本框不参与文字绕排）
- 无 Word 级样式集、母版、域自动更新
- DOCX **有损近似**，复杂文档可能丢格式
- **引擎**：覆盖正文 / 表 / 图分页与四周型绕排（预览/PDF）；分栏、脚注、复杂表跨页（单元格拆分）等未做或不完整
- **分页编辑器**：打字走增量分页（只重排光标页附近），停止输入 120ms 后补齐精确页数与滚动条，打字期间页码/页脚为估算值；与 PDF 行边界可能有细微差别。超长单段落文档（HTML 导入）会自动规范化重建，避免编辑开销退化为 O(n)；图片资源加载时自动降采样（最长边 ≤2048px），重绘走像素图缓存
- **网格线**：页面视图可显示 5mm 辅助网格（视图菜单开关）
- **页眉页脚**：通过对话框编辑，分页编辑器按设置绘制在每页；首页不同/奇偶页以对话框为准。DOCX 读写为纯文本 + PAGE 域
- 当前面向 **macOS**（PDF 预览依赖系统框架；拼写检查用内置 Hunspell + en_US 词典，可跨平台）

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

依赖：Qt 6.11+（Widgets + PrintSupport）、macOS、zlib。第三方库：**minizip-ng**（DOCX 的 ZIP 读写）、**libwebp**（WebP 图片解码）、**Hunspell**（拼写检查），macOS 上可用 Homebrew 安装：

```bash
brew install minizip-ng webp hunspell
```

词典（en_US.aff/.dic，MIT 许可）已随仓库打包在 `resources/dictionaries/`，构建时自动复制进 App Bundle 的 `Resources/dictionaries`；运行时也可通过 `HUNSPELL_DICT_PATH` 环境变量指定自定义词典目录。

可选：Python 3 + venv（DOCX 增强）。

```bash
# 开发构建
cmake --preset qt6-macos-debug
cmake --build --preset qt6-macos-debug
open build/NewWord.app

# 单元测试（ctest：DOCX 往返、LayoutEngine 分页、自绘分页编辑器渲染/跨页/输入、性能基准）
# 测试按主题拆分：tst_coresmoke_docx / _engine / _editor（共享 coresmoke.h）+ tst_documenttab_smoke；
# 全部在 offscreen 平台运行（含窗口渲染类用例）
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
- 毕设第 4～5 章提纲：[`docs/thesis_ch4_ch5_outline.md`](docs/thesis_ch4_ch5_outline.md)（v2：自绘分页编辑器 + 增量分页 + 性能评测）
- 毕设第 4～5 章正文：[`docs/thesis_ch4_ch5_body.md`](docs/thesis_ch4_ch5_body.md)（4 章：布局引擎与自绘分页编辑器设计；5 章：实验评测与结果分析）

> 说明：`pagination_eval.md` 是早期「引擎驱动活页页数」方案的评测（阶段 1 成果）；当前编辑视图已改为 QTextDocument 自绘分页，页数口径与导出不同源，详细修订见论文提纲 5.5。

## 关于框

菜单 **帮助 → 关于 NewWord**：版本 **0.5**、Qt 编辑 + LayoutEngine 预览/打印、DOCX 路径状态。
