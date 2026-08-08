# 验收样例

`demo.docx`：标题 + 正文 + 表格，用于 v0.5 主路径验收。

```text
打开 demo.docx → 改几个字 → 保存 → 分页预览 / 导出 PDF
```

预览与 PDF 由 `LayoutEngine` 分页；活页编辑区页缝仍为近似，页数可能略有差异。

重新生成（开发时）：

```bash
cmake --build --preset qt6-macos-debug --target NewWordMakeDemo
```
