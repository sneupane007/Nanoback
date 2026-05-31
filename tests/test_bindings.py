"""
Smoke test for the nanoback PyBind11 module.

Run from the project root after building:
    cmake -B build && cmake --build build
    python3 tests/test_bindings.py
"""
import sys
import os

sys.path.insert(0, os.path.abspath("build"))

try:
    import nanoback
except ImportError as e:
    print(f"FAIL: Could not import nanoback — {e}")
    print("Make sure you have run: cmake -B build && cmake --build build")
    sys.exit(1)

TESTS_PASSED = 0
TESTS_FAILED = 0

def check(condition: bool, name: str, detail: str = ""):
    global TESTS_PASSED, TESTS_FAILED
    if condition:
        print(f"  PASS  {name}")
        TESTS_PASSED += 1
    else:
        print(f"  FAIL  {name}" + (f" — {detail}" if detail else ""))
        TESTS_FAILED += 1

# ---------------------------------------------------------------------------
print("\n=== SMA Crossover (AAPL) ===")
r = nanoback.run_backtest("data/AAPL.csv", "AAPL", "sma", 10, 30, 0.0)

check(r.total_bars > 0,
      "total_bars > 0", f"got {r.total_bars}")
check(len(r.equity_curve) == r.total_bars,
      "equity_curve length == total_bars")
check(len(r.market_data) == r.total_bars,
      "market_data length == total_bars")
check(len(r.timestamps) == r.total_bars,
      "timestamps length == total_bars")
check(r.total_trades >= 0,
      "total_trades >= 0", f"got {r.total_trades}")
check(len(r.trade_pnls) == r.total_trades,
      "trade_pnls length == total_trades")
check(not (r.sharpe_ratio != r.sharpe_ratio),   # NaN check
      "sharpe_ratio is not NaN")
check(r.max_drawdown >= 0,
      "max_drawdown >= 0", f"got {r.max_drawdown}")
check(0 <= r.win_rate <= 100,
      "win_rate in [0, 100]", f"got {r.win_rate}")

# Check OHLCVBar fields are accessible
bar = r.market_data[0]
check(bar.close > 0,
      "market_data[0].close > 0", f"got {bar.close}")
check(bar.high >= bar.low,
      "market_data[0].high >= low")

# Check signal bar_index is in range
for sig in r.signals:
    check(0 <= sig.bar_index < r.total_bars,
          f"signal bar_index {sig.bar_index} in [0, {r.total_bars})")
    check(sig.direction in ("LONG", "EXIT"),
          f"signal direction is LONG or EXIT, got '{sig.direction}'")
    break  # check only first signal to keep output concise

# ---------------------------------------------------------------------------
print("\n=== RSI (AAPL) ===")
r2 = nanoback.run_backtest("data/AAPL.csv", "AAPL", "rsi", 14, 0, 70.0)
check(r2.total_bars == r.total_bars,
      "same bar count for same CSV", f"sma={r.total_bars} rsi={r2.total_bars}")

# ---------------------------------------------------------------------------
print("\n=== Mean Reversion (AAPL) ===")
r3 = nanoback.run_backtest("data/AAPL.csv", "AAPL", "mean_reversion", 20, 0, 2.0)
check(r3.total_bars == r.total_bars, "same bar count")

# ---------------------------------------------------------------------------
print(f"\n{'='*40}")
print(f"Results  PASS: {TESTS_PASSED}  FAIL: {TESTS_FAILED}")
print(f"{'='*40}")
print(f"\nSMA  — {r.total_bars} bars, {r.total_trades} trades, "
      f"return={r.total_return:+.2f}%, sharpe={r.sharpe_ratio:.3f}, "
      f"drawdown={r.max_drawdown:.2f}%")
print(f"RSI  — {r2.total_trades} trades, return={r2.total_return:+.2f}%")
print(f"MR   — {r3.total_trades} trades, return={r3.total_return:+.2f}%")

if TESTS_FAILED:
    sys.exit(1)
