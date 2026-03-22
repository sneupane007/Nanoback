#!/usr/bin/env bash
# run.sh — Configure, build, and run Nanoback from the project root.
#
# Usage:
#   ./run.sh                        # uses default: data/AAPL.csv AAPL
#   ./run.sh data/MSFT.csv MSFT     # custom CSV and ticker

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

CSV="${1:-data/AAPL_tick_data.csv}"
TICKER="${2:-AAPL}"

# ── 1. Configure ──────────────────────────────────────────────────────────────
echo "==> Configuring..."
cmake -B "$BUILD_DIR" -S "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release --log-level=ERROR

# ── 2. Build ──────────────────────────────────────────────────────────────────
echo "==> Building..."
cmake --build "$BUILD_DIR" -- -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

# ── 3. Run ────────────────────────────────────────────────────────────────────
echo "==> Running: $BUILD_DIR/Backtester $CSV $TICKER"
echo ""
"$BUILD_DIR/Backtester" "$CSV" "$TICKER"
