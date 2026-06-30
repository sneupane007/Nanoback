# Nanoback

Low-latency, memory-efficient backtesting engine with a C++17 core and Python data tooling.

```
Strategy: sma
Data source: CSV

=== Backtest Performance Report ===
Total Return : -9.36%
Sharpe Ratio : -0.0724
Max Drawdown : 27.92%
Win Rate     : 37.84%
Total Trades : 37
Total Bars   : 1736

=== Timing ===
Bars (MarketEvents) : 1736
Events processed    : 1958
Parse  (data ingest): 1.23 ms  (1,410,569 bars/s)
Process (cascade)   : 0.05 ms  (34,720,000 bars/s)
```

---

## Quick Start

```bash
# Prerequisites: C++17 compiler, CMake, protobuf
brew install protobuf          # macOS — skip if already installed

# Build (from project root)
cmake -B build && cmake --build build

# Install Python dependencies
pip install -r requirements.txt
```

---

## Step 1 — Download Market Data

Edit the config block at the bottom of `data/data.py` and run it:

```python
# data/data.py — edit these four variables, then run
SYMBOL         = "AAPL"    # Yahoo Finance ticker: 'AAPL', 'BTC-USD', 'EURUSD=X', etc.
TIMEFRAME      = "1d"      # Bar resolution (see table below)
RETRIEVAL_RANGE = "max"    # How far back to fetch
FILE_NAME      = "AAPL.csv"
```

```bash
python3 data/data.py
# → writes data/AAPL.csv  (or wherever FILE_NAME points)
```

### Timeframe / range reference

| `TIMEFRAME` | Max `RETRIEVAL_RANGE` | Best for |
|---|---|---|
| `1m` | last 7 days only | Scalping, ORB |
| `5m`, `15m`, `30m` | last 60 days | Intraday strategies |
| `1h` | last 730 days | VWAP, intraday swing |
| `1d` | `max` (decades) | SMA, RSI, momentum, mean reversion |
| `1wk`, `1mo` | `max` | Long-term trend following |

> For intraday strategies (`scalping`, `orb`, `vwap`) use `TIMEFRAME="1h"` and `RETRIEVAL_RANGE="2y"`. The `1m` limit of 7 days is too short for meaningful backtests.

---

## Step 2 — Choose a Data Source

Nanoback has two data handlers that implement the same `IDataHandler` interface. The event loop is identical regardless of which one you use — only the ingestion layer changes.

### Option A — CSV (default)

No extra steps. Pass the CSV path directly to the backtester:

```bash
./build/Backtester data/AAPL.csv AAPL
```

### Option B — Protobuf binary (faster ingestion, no text parsing)

CSV parsing dominates the CSV path by ~80× over the engine itself (2,275 ns/bar to parse vs. 27 ns/bar to run the full strategy→portfolio→execution cascade). The protobuf path eliminates all text parsing.

**One-time setup** — generate the Python protobuf bindings (run once from the project root):

```bash
/opt/homebrew/bin/protoc --python_out=data proto/ohlcv.proto
# → writes data/ohlcv_pb2.py
```

**Per-file conversion** — convert a CSV to a length-delimited binary `.pb`:

```bash
python3 data/csv_to_proto.py data/AAPL.csv AAPL data/AAPL.pb
# → Wrote 11466 bars to data/AAPL.pb
# → Size: 892,134 bytes (CSV) → 481,572 bytes (.pb)  (53.9%)
```

**Run with `--proto`**:

```bash
./build/Backtester data/AAPL.pb AAPL --proto
```

---

## Step 3 — Choose a Strategy

Pass `--strategy <name>` to select a strategy. The default is `sma`.

```bash
./build/Backtester data/AAPL.csv AAPL --strategy rsi --p1 14 --fp 70.0
```

### Strategy reference

| `--strategy` | Logic | `--p1` | `--p2` | `--fp` | Defaults |
|---|---|---|---|---|---|
| `sma` | SMA crossover: LONG when fast > slow | fast period | slow period | — | 10 / 30 |
| `rsi` | RSI oversold/overbought: LONG < 30, EXIT > overbought | RSI period | — | overbought level | 14 / — / 70.0 |
| `mean_reversion` | Bollinger-band reversion: LONG below −k σ, EXIT above +k σ | lookback period | — | k (band width) | 20 / — / 2.0 |
| `momentum` | Rate-of-change: LONG when ROC > +threshold, EXIT when < −threshold | ROC period | — | threshold (%) | 10 / — / 1.0 |
| `scalping` | EMA crossover (short-term): LONG on golden cross, EXIT on death cross | fast EMA period | slow EMA period | — | 5 / 13 |
| `breakout` | N-period channel breakout: LONG above high, EXIT below low | channel period | — | — | 20 |
| `pullback` | Trend-following pullback entry: LONG when trend is UP and price pulls back to short EMA | trend EMA period | pullback EMA period | — | 50 / 10 |
| `vwap` | VWAP mean-reversion: LONG below VWAP, EXIT above | VWAP period | — | — | 20 |
| `orb` | Opening range breakout: builds range over first N bars, then trades breakouts per cycle | range bars | cycle bars | — | 5 / 30 |

### Examples

```bash
# SMA with custom periods
./build/Backtester data/AAPL.csv AAPL --strategy sma --p1 20 --p2 50

# RSI with tighter overbought threshold
./build/Backtester data/AAPL.csv AAPL --strategy rsi --p1 10 --fp 65.0

# Bollinger Band mean reversion, wider bands
./build/Backtester data/AAPL.csv AAPL --strategy mean_reversion --p1 20 --fp 2.5

# Scalping on hourly data via protobuf
./build/Backtester data/AAPL.pb AAPL --proto --strategy scalping --p1 5 --p2 13

# ORB with a 10-bar opening range, 60-bar cycle
./build/Backtester data/AAPL.csv AAPL --strategy orb --p1 10 --p2 60
```

---

## Full CLI Reference

```
./build/Backtester <data_path> <ticker> [options]

Arguments:
  <data_path>          Path to CSV file or .pb binary
  <ticker>             Ticker symbol (e.g. AAPL, BTC-USD)

Options:
  --proto              Read data_path as a length-delimited protobuf binary
                       (default: treat as CSV)
  --strategy <name>    Strategy to run (default: sma)
                       Valid: sma | rsi | mean_reversion | momentum |
                              scalping | breakout | pullback | vwap | orb
  --p1 <int>           First integer parameter (strategy-dependent, see table above)
  --p2 <int>           Second integer parameter (strategy-dependent, see table above)
  --fp <float>         Floating-point parameter (strategy-dependent, see table above)
```

### Interactive runner (prompts for CSV path + ticker)

```bash
./run.sh [--strategy <name>] [--p1 N] [--p2 N] [--fp F]
```

---

## Running Tests

```bash
# All tests
cd build && ctest --output-on-failure

# Individual test binaries (verbose)
./build/tests/test_event_queue
./build/tests/test_data_handler
./build/tests/test_proto_data_handler
./build/tests/test_strategy
./build/tests/test_portfolio
./build/tests/test_execution
./build/tests/test_performance
./build/tests/test_live_session
```

---

## Performance

The engine is fast where it was designed to be: the event-driven cascade
(Strategy → Portfolio → Execution → Portfolio) costs only **~27 ns per bar**.
End-to-end profiling on a 2M-bar file showed the real cost lives entirely in
**CSV ingestion** — parsing dominates the cascade by roughly **80×**.

### CSV parse optimization: `timegm` → `days_from_civil`

An A/B test (identical OHLCV, only the timestamp column changed) isolated where
the parse time went:

| Timestamp encoding | Parse (2M bars) | Wall clock |
|--------------------|-----------------|------------|
| Date string `2000-01-01 00:00:00` (`timegm`)      | ~4,700 ms | ~4,825 ms |
| Integer epoch `946684800`                          | ~473 ms   | ~591 ms   |

The gap proved that **date-string conversion was ~90% of parse time** — a single
per-bar `timegm()` libc call burning ~2 µs each, 2,000,000 times. Replacing it
with Howard Hinnant's branchless `days_from_civil` (a dozen integer ops, no libc,
no locale) collapsed that cost:

| Phase (2M date-string bars, warm cache) | Before (`timegm`) | After (`days_from_civil`) | Speedup |
|------------------------------------------|-------------------|---------------------------|---------|
| Parse      | ~4,700 ms | **~700 ms** | **~6.7×** |
| Wall clock | ~4,825 ms | **~930 ms** | **~5.2×** |

Parse throughput rose from ~0.42M to **~2.8M bars/s**. The new path was verified
byte-identical to `timegm` across 74,511 dates (1971–2038, every leap year and
month boundary), zero mismatches.

### Parse-in-place: removing per-bar allocations

With `timegm` gone, the residual cost was the tokenizer itself — it built a
`std::vector<std::string>` and `substr`'d each field (~7 heap allocations per
bar). Replacing that with non-owning `std::string_view` spans into the line
buffer (zero allocations, fed straight to `std::from_chars`) closed most of the
remaining gap to the ~473 ms "epoch floor":

| Phase (2M date-string bars, warm cache) | `days_from_civil` | + parse-in-place | Total vs. original |
|------------------------------------------|-------------------|------------------|--------------------|
| Parse      | ~700 ms | **~522 ms** | **~9× faster** (from ~4,700 ms) |
| Wall clock | ~930 ms | **~738 ms** | **~6.5× faster** (from ~4,825 ms) |

Parse throughput is now **~3.83M bars/s**.

### Protobuf binary ingestion

The `--proto` path reads a pre-converted `.pb` binary (produced by `data/csv_to_proto.py`).
Each bar is a varint-length-prefixed serialized `OhlcvBar` proto message — no text parsing,
no `strtod`, no allocations beyond the per-bar deserialization buffer. This path pushes
ingestion toward the ~27 ns/bar engine processing floor.
