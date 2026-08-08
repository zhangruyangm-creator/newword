#!/usr/bin/env python3
"""DOCX bridge for NewWord.

Import uses mammoth (DOCX → HTML).
Export uses html2docx (HTML → DOCX).

This is intentionally lossy vs Microsoft Word, but much stronger than a
hand-rolled OOXML subset for body text, headings, lists, tables, and images.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def import_docx(docx_path: Path, html_path: Path) -> int:
    import mammoth

    with docx_path.open("rb") as src:
        result = mammoth.convert_to_html(src)

    html_path.write_text(result.value or "", encoding="utf-8")
    for message in result.messages:
        print(f"[mammoth] {message}", file=sys.stderr)
    return 0


def export_docx(html_path: Path, docx_path: Path, title: str) -> int:
    from html2docx import html2docx

    html = html_path.read_text(encoding="utf-8")
    lowered = html.lower()
    if "<html" not in lowered:
        html = (
            "<!DOCTYPE html><html><head>"
            '<meta charset="utf-8">'
            f"<title>{title}</title>"
            "</head><body>"
            f"{html}"
            "</body></html>"
        )

    buf = html2docx(html, title=title or "NewWord")
    docx_path.write_bytes(buf.getvalue())
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="NewWord DOCX bridge")
    sub = parser.add_subparsers(dest="command", required=True)

    p_import = sub.add_parser("import", help="DOCX → HTML")
    p_import.add_argument("docx")
    p_import.add_argument("html")

    p_export = sub.add_parser("export", help="HTML → DOCX")
    p_export.add_argument("html")
    p_export.add_argument("docx")
    p_export.add_argument("--title", default="NewWord")

    p_check = sub.add_parser("check", help="Verify dependencies")

    args = parser.parse_args(argv)

    if args.command == "check":
        import mammoth  # noqa: F401
        from html2docx import html2docx  # noqa: F401

        print("docx-bridge-ok")
        return 0

    if args.command == "import":
        return import_docx(Path(args.docx), Path(args.html))
    if args.command == "export":
        return export_docx(Path(args.html), Path(args.docx), args.title)
    return 2


if __name__ == "__main__":
    sys.exit(main())
