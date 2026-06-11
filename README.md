# Nanoback
low latency, and memory efficient backtesting engine built with C++ core and Python for data visualisation. 

```
Strategy: sma

=== Backtest Performance Report ===
Total Return : -9.36%
Sharpe Ratio : -0.0724
Max Drawdown : 27.92%
Win Rate     : 37.84%
Total Trades : 37
Total Bars   : 1736

=== Timing ===
Events processed : 1958
Total time       : 91304 ns
Avg per event    : 46 ns
```

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
