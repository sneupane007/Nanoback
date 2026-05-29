#!/usr/bin/env bash
# Usage: ./run.sh [--strategy sma|rsi|mean_reversion|...] [--p1 N] [--p2 N] [--fp F]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cmake -B "$ROOT/build" -S "$ROOT" --log-level=ERROR -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -- -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

echo ""
read -rp "Dataset path: " CSV
read -rp "Ticker:       " TICKER

"$ROOT/build/Backtester" "$CSV" "$TICKER" "$@"
