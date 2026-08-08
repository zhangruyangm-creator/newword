#!/usr/bin/env bash
# Create/update the project-local venv used by NewWord's DOCX bridge.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VENV="$ROOT/.venv"
PYTHON="${PYTHON:-python3}"

"$PYTHON" -m venv "$VENV"
"$VENV/bin/pip" install -U pip
"$VENV/bin/pip" install -r "$ROOT/tools/docx_bridge/requirements.txt"
"$VENV/bin/python" "$ROOT/tools/docx_bridge/docx_bridge.py" check
echo "DOCX bridge ready: $VENV"
