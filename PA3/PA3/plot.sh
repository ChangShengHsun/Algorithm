#!/usr/bin/env bash
set -euo pipefail

CASE=${CASE:-case2}         # cap/net 檔名（不含副檔名）
ROUTE_CASE=${ROUTE_CASE:-case2}  # 要看的 route 檔名（不含副檔名）
OUT_HTML=${OUT_HTML:-"$PWD/outputs/out-${CASE}.html"}  # 輸出 HTML/JS 檔

ROOT="$(cd "$(dirname "$0")" && pwd)"
CAP="$ROOT/inputs/${CASE}.cap"
NET="$ROOT/inputs/${CASE}.net"
ROUTE="$ROOT/outputs/${ROUTE_CASE}.route"

python3 "$ROOT/utilities/export_plotly.py" \
  --cap "$CAP" --net "$NET" --route "$ROUTE" --out "$OUT_HTML"
echo "Plot written to $OUT_HTML"
