#!/usr/bin/env bash
# Usage: ./run.sh   (fully interactive — no flags needed)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── Build ──────────────────────────────────────────────────────────────────
cmake -B "$ROOT/build" -S "$ROOT" --log-level=ERROR -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -- -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"
echo ""

# ── Stage 1: Dataset ────────────────────────────────────────────────────────
shopt -s nullglob
csvs=( "$ROOT"/data/*.csv )
shopt -u nullglob

if [[ ${#csvs[@]} -eq 0 ]]; then
    echo "Error: no .csv files found in $ROOT/data/" >&2
    exit 1
fi

echo "Available datasets:"
for i in "${!csvs[@]}"; do
    echo "  [$((i+1))] $(basename "${csvs[$i]}")"
done

while true; do
    read -rp "Choose dataset [1-${#csvs[@]}]: " ds_input
    ds_input="${ds_input:-1}"
    if [[ "$ds_input" =~ ^[0-9]+$ ]] && (( ds_input >= 1 && ds_input <= ${#csvs[@]} )); then
        break
    fi
    echo "  Please enter a number between 1 and ${#csvs[@]}."
done

CSV="${csvs[$((ds_input-1))]}"
TICKER="$(basename "$CSV" .csv | tr '[:lower:]' '[:upper:]')"
echo "  → $CSV  (ticker: $TICKER)"
echo ""

# ── Stage 2: Data format ────────────────────────────────────────────────────
echo "Data format:"
echo "  [1] CSV      (text parsing, always available)"
echo "  [2] Protobuf (binary, faster)"
read -rp "Choose [1-2, default 1]: " fmt_input
fmt_input="${fmt_input:-1}"

PROTO_FLAG=""
if [[ "$fmt_input" == "2" ]]; then
    PB="$ROOT/data/${TICKER}.pb"
    if [[ -f "$PB" ]]; then
        echo "  → Using cached $PB"
    else
        echo "  → Generating $PB ..."
        python3 "$ROOT/data/csv_to_proto.py" "$CSV" "$TICKER" "$PB"
        echo "  → Done."
    fi
    DATA_ARG="$PB"
    PROTO_FLAG="--proto"
else
    DATA_ARG="$CSV"
    echo "  → CSV mode"
fi
echo ""

# ── Stage 3: Strategy ───────────────────────────────────────────────────────
strategy_names=(sma rsi mean_reversion momentum scalping breakout pullback vwap orb)
strategy_descs=(
    "SMA crossover (fast/slow moving averages)"
    "RSI overbought/oversold signals"
    "Bollinger Band mean-revert"
    "Price momentum over a window"
    "Fast scalp (5/13 MA crossover)"
    "Donchian channel breakout"
    "Trend + pullback entry"
    "VWAP deviation signal"
    "Opening Range Breakout"
)

echo "Strategies:"
for i in "${!strategy_names[@]}"; do
    printf "  [%d] %-16s %s\n" "$((i+1))" "${strategy_names[$i]}" "${strategy_descs[$i]}"
done

while true; do
    read -rp "Choose strategy [1-${#strategy_names[@]}, default 1]: " strat_input
    strat_input="${strat_input:-1}"
    if [[ "$strat_input" =~ ^[0-9]+$ ]] && (( strat_input >= 1 && strat_input <= ${#strategy_names[@]} )); then
        break
    fi
    echo "  Please enter a number between 1 and ${#strategy_names[@]}."
done

STRATEGY="${strategy_names[$((strat_input-1))]}"
echo "  → Strategy: $STRATEGY"
echo ""

# ── Stage 4: Parameters ─────────────────────────────────────────────────────
P1="" ; P2="" ; FP=""

read_param() {
    local label="$1" default="$2" varname="$3"
    read -rp "  $label [$default]: " tmp
    if [[ -n "${tmp:-}" && "$tmp" != "$default" ]]; then
        printf -v "$varname" '%s' "$tmp"
    fi
}

case "$STRATEGY" in
    sma)            read_param "Fast period"          "10"   P1
                    read_param "Slow period"          "30"   P2 ;;
    rsi)            read_param "Period"               "14"   P1
                    read_param "Overbought threshold" "70.0" FP ;;
    mean_reversion) read_param "Period"               "20"   P1
                    read_param "Bollinger k"          "2.0"  FP ;;
    momentum)       read_param "Period"               "10"   P1
                    read_param "Threshold %"          "1.0"  FP ;;
    scalping)       read_param "Fast period"          "5"    P1
                    read_param "Slow period"          "13"   P2 ;;
    breakout)       read_param "Lookback period"      "20"   P1 ;;
    pullback)       read_param "Trend period"         "50"   P1
                    read_param "Pullback period"      "10"   P2 ;;
    vwap)           read_param "Period"               "20"   P1 ;;
    orb)            read_param "Range bars"           "5"    P1
                    read_param "Cycle bars"           "30"   P2 ;;
esac

# ── Assemble and run ────────────────────────────────────────────────────────
ARGS=("$DATA_ARG" "$TICKER" "--strategy" "$STRATEGY")
[[ -n "$PROTO_FLAG" ]] && ARGS+=("$PROTO_FLAG")
[[ -n "${P1:-}" ]]     && ARGS+=("--p1" "$P1")
[[ -n "${P2:-}" ]]     && ARGS+=("--p2" "$P2")
[[ -n "${FP:-}" ]]     && ARGS+=("--fp" "$FP")

echo ""
echo "Running: $ROOT/build/Backtester ${ARGS[*]}"
echo "────────────────────────────────────────────────────────────────────────"
"$ROOT/build/Backtester" "${ARGS[@]}"
