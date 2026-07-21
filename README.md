# Nanoback

Low-latency, memory-efficient backtesting engine with a C++17 core and Python data tooling.

## At a Glance

Nanoback is not a script that loops over a DataFrame — it's an **event-driven engine** built the way production trading infrastructure is built:

- **Event-sourced architecture** — `DataHandler`, `Strategy`, `Portfolio`, `ExecutionHandler`, and `PerformanceTracker` never call each other directly. They communicate only through typed events (`std::variant<MarketEvent, SignalEvent, OrderEvent, FillEvent>`) pushed through a shared queue, so any component can be swapped without touching the others.
- **Zero-allocation hot path** — the event queue is a pre-allocated ring buffer (`std::vector<Event>`, default 1,000,000 slots); no heap allocation happens during the backtest loop itself.
- **Binary ingestion path** — a Protocol Buffers schema (`proto/ohlcv.proto`) + a hand-rolled varint-framed reader (`ProtoDataHandler`) skip text parsing entirely, cutting ingest time roughly in half over CSV (see [Performance](#performance)).
- **C++/Python interop via pybind11** — the same engine that powers the CLI is exposed as a native Python extension module (`nanoback.*.so`), so a Streamlit UI can drive thousands of C++ backtests without a subprocess or a rewrite.
- **Multithreaded parameter sweeps** — `run_batch()` fans a parameter grid out across `std::thread::hardware_concurrency()` threads via `std::async`, each with its own isolated engine instance (no locking needed). ~4.9× measured speedup on a 10-core machine (below).
- **9 pluggable strategies** — SMA/EMA crossover, RSI, Bollinger mean-reversion, momentum, breakout, pullback, VWAP, and opening-range breakout, all behind one `IStrategy` interface.
- **Test coverage** — one doctest executable per component (event queue, both data handlers, strategy, portfolio, execution, performance, live session, batch runner).

Deeper architectural writeup: [`ARCHITECTURE.md`](ARCHITECTURE.md).

```
$ ./build/Backtester data/AAPL.csv AAPL --strategy sma
Strategy: sma
Data source: CSV
sizeof(MarketEvent)=72  sizeof(Event)=80  buffer_bytes=81920

=== Backtest Performance Report ===
Total Return : -7.34%
Sharpe Ratio : 0.1553
Max Drawdown : 99.00%
Win Rate     : 40.00%
Total Trades : 200
Total Bars   : 11466

=== Timing ===
Bars (MarketEvents) : 11466
Events processed    : 12669
Peak queue depth    : 1
Parse  (data ingest): 2.56 ms  (4484.90 k bars/s)
Process (cascade)   : 0.31 ms  (37240.21 k bars/s)
Wall clock          : 3.22 ms  (3557.78 k bars/s)
Parse share of wall : 79.33 %
```
*(measured on this machine — Apple M4, 10 cores — see [Performance](#performance) for methodology)*

---

## Two Ways to Run

| | CLI (`./build/Backtester`) | Streamlit UI (`ui/app.py`) |
|---|---|---|
| Good for | One backtest, one config, fast iteration | Sweeping a parameter grid and comparing results visually |
| Runs | A single `run_backtest()` call | `run_batch()` — many configs in parallel across threads |
| Output | Text report + timing to stdout | Per-metric heatmaps (Sharpe, win rate, drawdown) + a weighted composite ranking |
| Requires | C++ build only | C++ build **+** the `nanoback` pybind11 module **+** a Python venv |

Both share the same engine and the same build step — the Streamlit path just needs the extra pybind11 module on top.

---

## Quick Start

### Option 1 — CLI only

```bash
# Prerequisites: C++17 compiler, CMake, protobuf
brew install protobuf          # macOS — skip if already installed

# Build (from project root)
cmake -B build && cmake --build build

# Run
./build/Backtester data/AAPL.csv AAPL
```

### Option 2 — CLI + Streamlit parallel sweep UI

The Python module (`nanoback.*.so`) must be built under the **same interpreter** Streamlit runs with, or the import fails with an ABI mismatch (e.g. a `cpython-39` `.so` that a Python 3.14 interpreter can't load). Use one venv for both:

```bash
brew install protobuf          # macOS — skip if already installed

python3 -m venv .venv
./.venv/bin/pip install -r requirements.txt

cmake -B build -DPYBIND11_FINDPYTHON=ON \
      -Dpybind11_DIR=$(./.venv/bin/python -m pybind11 --cmakedir) \
      -DPython_EXECUTABLE=$(pwd)/.venv/bin/python
cmake --build build -j          # produces build/nanoback.cpython-3XX-*.so

./.venv/bin/streamlit run ui/app.py
```

This opens a browser tab where you pick a strategy, a parameter grid, and one or more tickers (any `.csv` in `data/`), then click **Run sweep** — every combination runs in parallel and results land in Sharpe/win-rate/drawdown/composite heatmaps.

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

Measured on this machine: CSV ingest runs ~4.5M bars/s; the protobuf path runs ~2.4x faster ingest at ~10.6M bars/s by skipping text parsing entirely (see [Performance](#performance)).

**One-time setup** — generate the Python protobuf bindings (run once from the project root):

```bash
/opt/homebrew/bin/protoc --python_out=data proto/ohlcv.proto
# → writes data/ohlcv_pb2.py
```

**Per-file conversion** — convert a CSV to a length-delimited binary `.pb`:

```bash
python3 data/csv_to_proto.py data/AAPL.csv AAPL data/AAPL.pb
# → Wrote 11466 bars to data/AAPL.pb
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

The CLI binary runs **one config per invocation** — there is no `--sweep` flag. Parameter sweeps run through the `nanoback` Python module instead (below).

### Interactive runner (prompts for CSV path + ticker)

```bash
./run.sh [--strategy <name>] [--p1 N] [--p2 N] [--fp F]
```

---

## Running a Parallel Sweep

There's no sweep mode in the C++ CLI binary — a sweep is a set of independent `BacktestConfig`s fanned out through `run_batch()`, which only exists on the Python side (`bindings/nanoback_bindings.cpp`). Two ways to drive it:

### Via Streamlit (recommended — visual heatmaps)

```bash
./.venv/bin/streamlit run ui/app.py
```

Pick a strategy, set start/stop/step for each parameter axis, select ticker(s), click **Run sweep**. Results render as Sharpe/win-rate/drawdown heatmaps plus a composite score you can re-weight live (cached, so re-weighting doesn't re-run the C++ side).

### Via a raw Python script (no UI)

```python
import sys
sys.path.insert(0, "build")
import nanoback as nb

configs = []
for fast in range(5, 25, 5):
    for slow in range(20, 60, 10):
        c = nb.BacktestConfig()
        c.data_path, c.ticker, c.strategy_name = "data/AAPL.csv", "AAPL", "sma"
        c.p1, c.p2 = fast, slow
        configs.append(c)

results = nb.run_batch(configs)   # parallel fan-out, one thread wave per hardware_concurrency()
for r in results:
    print(r.p1, r.p2, r.sharpe_ratio, r.max_drawdown)
```

Requires the same pybind11 build as the Streamlit path (Option 2 in Quick Start).

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
./build/tests/test_run_backtest
```

---

## Performance

All numbers below marked *(this machine)* were measured freshly on **Apple M4, 10 cores**, on the repo's real `data/AAPL.csv` (11,466 daily bars). Numbers will differ on other hardware — re-run the commands shown to get your own.

### Ingest: CSV vs protobuf *(this machine)*

5-run average, `./build/Backtester data/AAPL.csv AAPL` vs `./build/Backtester data/AAPL.pb AAPL --proto`:

| Path | Parse time | Parse throughput | Wall clock |
|---|---|---|---|
| CSV | ~2.5–2.65 ms | ~4.4M bars/s | ~3.2–3.3 ms |
| Protobuf | ~1.0–1.3 ms | ~10.6M bars/s | ~1.6–2.0 ms |

The strategy→portfolio→execution cascade itself is essentially free either way (~0.3 ms, ~37M bars/s) — ingestion is where the time goes, which is why the protobuf path exists.

### Parallel sweep speedup *(this machine)*

400 SMA configs (`p1` × `p2` grid) against `data/AAPL.csv`, via the `nanoback` Python module:

| Mode | Total time | Per-backtest |
|---|---|---|
| Serial (`run_backtest()` × 400) | 1343 ms | 3.36 ms |
| Parallel (`run_batch()`, `std::async` waves) | 272–301 ms | 0.68–0.75 ms |

**~4.9× speedup** on 10 hardware threads (Apple M4: 4 performance + 6 efficiency cores — sub-linear scaling is expected on a heterogeneous core mix, not a bug).

Reproduce with:
```bash
./.venv/bin/python -c "
import sys, time; sys.path.insert(0, 'build'); import nanoback as nb
configs = [nb.BacktestConfig() for _ in range(400)]
for i, c in enumerate(configs):
    c.data_path, c.ticker, c.strategy_name = 'data/AAPL.csv', 'AAPL', 'sma'
    c.p1, c.p2 = 5 + i // 20, 30 + i % 20
t0 = time.perf_counter(); nb.run_batch(configs); print((time.perf_counter()-t0)*1000, 'ms')
"
```

### Historical optimization case study (`timegm` → `days_from_civil`)

The following numbers are from an earlier development session on a 2M-bar synthetic dataset that isn't part of this repo — they document *why* the CSV parser is shaped the way it is, not a benchmark you can re-run today. The relative speedups are the durable result; treat the absolute millisecond figures as a snapshot of different hardware, not this machine.

An A/B test (identical OHLCV, only the timestamp column changed) isolated where CSV parse time went:

| Timestamp encoding | Parse (2M bars) | Wall clock |
|--------------------|-----------------|------------|
| Date string `2000-01-01 00:00:00` (`timegm`)      | ~4,700 ms | ~4,825 ms |
| Integer epoch `946684800`                          | ~473 ms   | ~591 ms   |

Date-string conversion was ~90% of parse time — a single per-bar `timegm()` libc call burning ~2 µs each, 2,000,000 times. Replacing it with Howard Hinnant's branchless `days_from_civil` (a dozen integer ops, no libc, no locale) collapsed that cost:

| Phase (2M date-string bars, warm cache) | Before (`timegm`) | After (`days_from_civil`) | Speedup |
|------------------------------------------|-------------------|---------------------------|---------|
| Parse      | ~4,700 ms | ~700 ms | ~6.7× |
| Wall clock | ~4,825 ms | ~930 ms | ~5.2× |

The new path was verified byte-identical to `timegm` across 74,511 dates (1971–2038, every leap year and month boundary), zero mismatches.

With `timegm` gone, the residual cost was the tokenizer itself — it built a `std::vector<std::string>` and `substr`'d each field (~7 heap allocations per bar). Replacing that with non-owning `std::string_view` spans into the line buffer (zero allocations, fed straight to `std::from_chars`) closed most of the remaining gap:

| Phase (2M date-string bars, warm cache) | `days_from_civil` | + parse-in-place | Total vs. original |
|------------------------------------------|-------------------|------------------|--------------------|
| Parse      | ~700 ms | ~522 ms | ~9× faster (from ~4,700 ms) |
| Wall clock | ~930 ms | ~738 ms | ~6.5× faster (from ~4,825 ms) |

### Protobuf binary ingestion

The `--proto` path reads a pre-converted `.pb` binary (produced by `data/csv_to_proto.py`). Each bar is a varint-length-prefixed serialized `OhlcvBar` proto message — no text parsing, no `strtod`, no allocations beyond the per-bar deserialization buffer. This is the fastest ingestion path available and the one to use for large sweeps.
