#!/usr/bin/env bash
set -euo pipefail

# 可用環境變數覆寫
CASE=${CASE:-case6}             # cap/net 檔名（不含副檔名）
ROUTE_CASE=${ROUTE_CASE:-$CASE} # 要檢查的 route 檔（不含副檔名）
PLOT_FLAG=${PLOT_FLAG:-0}       # 設 1 或 true 時會加上 -plot 產圖

ROOT="$(cd "$(dirname "$0")" && pwd)"
CAP="$ROOT/inputs/${CASE}.cap"
NET="$ROOT/inputs/${CASE}.net"
ROUTE="$ROOT/outputs/${ROUTE_CASE}.route"

CMD=(python3 "$ROOT/utilities/pa3_evaluator.py" "$CAP" "$NET" "$ROUTE")
if [[ "$PLOT_FLAG" == "1" || "$PLOT_FLAG" == "true" ]]; then
  CMD+=("-plot")
fi

"${CMD[@]}"
