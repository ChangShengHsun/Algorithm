#!/usr/bin/env bash
set -euo pipefail

CASE=${CASE:-case6}  # cap/net 檔名（不含副檔名）
ROOT="$(cd "$(dirname "$0")" && pwd)"
CAP="$ROOT/inputs/${CASE}.cap"
NET="$ROOT/inputs/${CASE}.net"
OUT="$ROOT/outputs/${CASE}.route"

cd "$ROOT"
"$ROOT/bin/pa3" --cap "$CAP" --net "$NET" --out "$OUT"
echo "Route written to $OUT"
