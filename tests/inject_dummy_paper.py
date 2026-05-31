#!/usr/bin/env python3
"""
Inject dummy OHLCV bars into a new paper trading session.
Simulates ~120 bars of realistic AAPL-like price movement so the
frontend shows a fully populated dashboard with equity curve,
candlestick chart, signals, and trade history.

Usage:
    python3 tests/inject_dummy_paper.py
"""
import math
import random
import time
import urllib.request
import urllib.parse
import json

BASE = "http://localhost:8000"
random.seed(42)

# ── Generate synthetic price data ─────────────────────────────────────────────
# Start at $185, add a slow upward trend with noise to trigger SMA crossovers
def generate_bars(n=140, start_price=185.0):
    bars = []
    price = start_price
    for i in range(n):
        # Gentle sine wave trend so fast SMA crosses slow SMA twice
        trend = 12 * math.sin(i / 40) + 0.05 * i
        noise = random.gauss(0, 0.6)
        close = price + trend / n + noise
        close = max(close, 1.0)  # never go negative

        spread = abs(random.gauss(0, 0.4))
        high   = close + spread + abs(random.gauss(0, 0.2))
        low    = close - spread - abs(random.gauss(0, 0.2))
        open_  = close + random.gauss(0, 0.25)
        high   = max(high, close, open_)
        low    = min(low,  close, open_)
        volume = int(random.randint(800_000, 5_000_000))
        # synthetic unix ts: each bar = 1 minute apart, starting 1h ago
        ts = int(time.time()) - (n - i) * 60

        bars.append((open_, high, low, close, volume, ts))
        price = close
    return bars


def post_json(path, body=None):
    url = BASE + path
    data = json.dumps(body).encode() if body else b""
    req  = urllib.request.Request(
        url, data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read())


def get_json(path):
    with urllib.request.urlopen(BASE + path, timeout=5) as resp:
        return json.loads(resp.read())


def tick(session_id, o, h, l, c, vol, ts):
    params = urllib.parse.urlencode({
        "open": o, "high": h, "low": l,
        "close": c, "volume": vol,
    })
    url = f"{BASE}/api/paper/{session_id}/tick?{params}"
    req = urllib.request.Request(url, method="POST")
    with urllib.request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read())


# ── Main ─────────────────────────────────────────────────────────────────────
def main():
    # 1. Health check
    health = get_json("/api/health")
    if not health.get("engine"):
        print("ERROR: engine not available —", health)
        return

    print("Engine available. Starting paper session…")

    # 2. Start session: SMA crossover p1=10, p2=30
    resp = post_json("/api/paper/start", {
        "ticker":          "AAPL",
        "strategy":        "sma",
        "p1":              10,
        "p2":              30,
        "fp":              0.0,
        "initial_capital": 100000.0,
        "interval":        "1m",
    })
    session_id = resp["session_id"]
    print(f"Session started: {session_id}")

    # 3. Inject bars
    bars = generate_bars(140)
    print(f"Injecting {len(bars)} bars…", end="", flush=True)
    state = None
    for i, (o, h, l, c, vol, ts) in enumerate(bars):
        state = tick(session_id, o, h, l, c, vol, ts)
        if (i + 1) % 20 == 0:
            print(f" {i+1}", end="", flush=True)
    print(" done.")

    # 4. Print summary
    if state:
        print(f"\n── Session Summary ──────────────────────────────")
        print(f"  Bars:         {state['bar_count']}")
        print(f"  Events:       {state['event_count']}")
        print(f"  Equity:       ${state['equity']:,.2f}")
        print(f"  Total Return: {state['total_return']:+.2f}%")
        print(f"  Sharpe:       {state['sharpe_ratio']:.3f}")
        print(f"  Max Drawdown: -{state['max_drawdown']:.2f}%")
        print(f"  Win Rate:     {state['win_rate']:.1f}%")
        print(f"  Total Trades: {state['total_trades']}")
        print(f"  Signals:      {len(state['signals'])}")
        print(f"\nOpen the frontend → PAPER TRADING tab")
        print(f"Session ID: {session_id}")
    else:
        print("No state returned — check server logs.")


if __name__ == "__main__":
    main()
