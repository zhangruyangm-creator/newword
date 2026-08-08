# 第 4～5 章提纲（毕设）

面向叙事：**轻量布局引擎 + 与预览/PDF 一致性验证**。  
对应实现：`engine/`（Model / Adapter / LayoutEngine / Painter / DocxExporter）与 [`pagination_eval.md`](pagination_eval.md)。

---

## 第 4 章 布局引擎设计与实现

### 4.1 问题与目标

- 双路径问题：编辑用 `QTextDocument`，若预览/PDF 另用几何估算，页数易分叉
- 目标：同一 `DocumentModel` 驱动预览、PDF、活页页数与页缝；不追求完整 WYSIWYG 自绘编辑器（阶段 3 刻意不做）

### 4.2 总体架构

```text
QTextDocument  ──QTextAdapter──►  DocumentModel
                                      ├─ LayoutEngine → LayoutPage[] → 预览 / PDF / 活页页数·页缝
                                      └─ DocxExporter → 内置 DOCX（阶段 2）
```

- 数据流单向：编辑缓冲仍是 Qt；Model 为布局与导出的规范视图
- 与 Word 级引擎的差异：无浮动环绕、无单元格内拆分、无分栏/脚注完整支持

### 4.3 DocumentModel

- 块：段落 / 表格；段落内 Run（文本、样式、原子图）
- 文档偏移（`documentPosition`）：页缝锚点基础
- 设计取舍：够用的排版语义，避免过早做成完整 DOM

### 4.4 QTextAdapter（适配层）

- `QTextDocument` → Model 的单向转换
- 覆盖：颜色、图片资源、表格单元格
- **导入不对称**：打开 DOCX 仍为桥接 / `DocxIO::load` → Qt；论文中明确「导出阶段 2、导入仍适配层」

### 4.5 LayoutEngine

- 断行：`QTextLayout`（与系统文本度量一致）
- 分页：按内容盒高度切页；表以**行**为原子；图为原子块
- 输出：`LayoutPage` 列表 + `startDocPos` / `pageBreakDocPositions`
- 局限：复杂表跨页（单元格纵向拆分）、分栏、脚注等未做或不完整

### 4.6 绘制与接线

- `LayoutPainter` / `PageDocumentPainter`：预览与 PDF
- `EditorViewLayout`：活页页数 = 引擎；页缝用文档偏移 → `cursorRect`
- 说明：行内断行仍可能由编辑器 Qt 排版决定，与 PDF 行边界可有细微差别（页**数**已对齐）

### 4.7 阶段 2：Model → DOCX

- `DocxExporter`：内置保存路径不经 HTML
- 与桥接（html2docx）的主备关系；失败回退策略
- 作为加分项：证明 Model 不仅服务分页，也服务持久化

### 4.8 本章小结

- 阶段 0～2 边界清晰；阶段 3（自绘编辑器）非本设计目标

---

## 第 5 章 实验评测与结果分析

### 5.1 评测目的

- 验证活页页数与 LayoutEngine（预览/PDF）一致
- 量化旧几何估算误差，说明换引擎的必要性

### 5.2 评测方法

- 指标三源：几何估算、活页(引擎)、LayoutEngine
- Δ活页 = 活页 − 引擎（接受标准：**恒为 0**）
- Δ几何 = 引擎 − 几何（短样例 ≤1；长文/图/多页表 ≤3）
- 可复现：`NewWordCoreTests::pagination_*` 生成 [`pagination_eval.md`](pagination_eval.md)

### 5.3 样例设计

| 类别 | 样例 | 考察点 |
|------|------|--------|
| 基线 | 空文档、短正文 | 单页无溢出 |
| 纯文本跨页 | 长纯文本 ×40 / ×80 | 几何易低估 |
| 样式 | 标题+正文 | 混合块 |
| 结构 | 含表格（小） | 表参与布局 |
| 富媒体 | 含图×4 | 大图原子跨页 |
| 结构跨页 | 多页表×36行 | 按行换页 |

### 5.4 结果与分析

- 引用评测表：强调 **Δ活页 全部为 0**
- 分析几何路径在长文/图/表上的偏差（典型差 1～3 页）→ 解释「为何不能继续用几何当活页页数」
- （可选插图）截取活页页缝与 PDF 首页对照；或附 `pagination_eval.md` 原文于附录

### 5.5 功能与正确性测试（简述）

- 适配：颜色 / 图片 / 表格单测
- 引擎：空页、长文分页、页缝偏移单调、Painter API 页数一致
- 导出：Model 导出往返（纯文本、样式、表）
- 产品侧：主题、字数统计等与主线弱相关，可一笔带过或放附录

### 5.6 威胁效度与局限

- 字体固定 Helvetica 12pt、A4；未覆盖全部中文字体度量差异
- 表不拆单元格；极复杂 Word 文档不在范围
- 导入路径未对称成 `DocxImporter ← Model`

### 5.7 本章小结

- 主线结论：引擎接线 + 可复现评测已支撑「一致性」主张
- 后续工作：导入对称、评测样例扩展、论文图表与演示稿

---

## 写作提示（落地用）

1. **第 4 章以架构图 + 模块职责为主**，少贴大段代码；关键接口用短清单（`layout` / `pageCount` / `pageBreakDocPositions`）
2. **第 5 章以表 `pagination_eval.md` 为核心证据**，文字解释「Δ活页=0」与「Δ几何≠0」的对比叙事
3. 阶段划分可放在 4.1 或绪论回顾：0 模型 → 1 预览/PDF → 1+ 活页对齐 → 2 导出 → 3 不做
4. 演示建议：打开长文 / 含图样例 → 导出 PDF → 对照页数与页缝

---

*与仓库实现同步；评测数字以测试重新生成的 `pagination_eval.md` 为准。*
