"""Nanoback — parallel parameter-sweep heatmap UI.

Pick a strategy, define a grid of parameters across one or more tickers, run every
combination in parallel (C++ threads via the `nanoback` PyBind11 module), and
review the results as per-metric heatmaps plus a weighted composite score.

Run from the project root under the same interpreter the `.so` was built with:
    streamlit run ui/app.py
"""

import itertools
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import plotly.express as px
import streamlit as st

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
DATA = ROOT / "data"

# Per-strategy sweep axes — mirrors src/strategy/strategy_factory.cpp exactly.
# Each axis: (config_field, label, kind, default_start, default_stop, default_step).
# `config_field` is the BacktestConfig attribute the axis drives (p1/p2/fp).
STRATS = {
    "sma":            [("p1", "fast", "int", 5, 15, 5),       ("p2", "slow", "int", 20, 40, 10)],
    "scalping":       [("p1", "fast", "int", 3, 9, 3),        ("p2", "slow", "int", 10, 20, 5)],
    "pullback":       [("p1", "trend", "int", 30, 60, 15),    ("p2", "pullback", "int", 5, 15, 5)],
    "orb":            [("p1", "range_bars", "int", 3, 9, 3),  ("p2", "cycle_bars", "int", 20, 40, 10)],
    "rsi":            [("p1", "period", "int", 7, 21, 7),     ("fp", "overbought", "float", 65.0, 75.0, 5.0)],
    "mean_reversion": [("p1", "period", "int", 10, 30, 10),   ("fp", "k", "float", 1.5, 2.5, 0.5)],
    "momentum":       [("p1", "period", "int", 5, 15, 5),     ("fp", "threshold", "float", 0.5, 1.5, 0.5)],
    "breakout":       [("p1", "period", "int", 10, 30, 10)],
    "vwap":           [("p1", "period", "int", 10, 30, 10)],
}


@st.cache_resource
def load_nanoback():
    """Import the compiled engine module once per server process."""
    if str(BUILD) not in sys.path:
        sys.path.insert(0, str(BUILD))
    import nanoback as nb
    return nb


def axis_values(kind, start, stop, step):
    """Inclusive value list from start..stop by step, typed int or float."""
    if step is None or step <= 0:
        return [int(start) if kind == "int" else float(start)]
    n = int(round((stop - start) / step)) + 1
    out = []
    for i in range(max(n, 1)):
        v = start + i * step
        out.append(int(round(v)) if kind == "int" else round(float(v), 6))
    seen, uniq = set(), []
    for v in out:  # dedupe, preserve order
        if v not in seen:
            seen.add(v)
            uniq.append(v)
    return uniq


@st.cache_data(show_spinner=False)
def run_sweep(strategy, tickers, axis_fields, values_per_axis):
    """Build the cartesian grid and run it through run_batch. Cached on the
    hashable inputs so composite-weight changes never re-run the C++ backtests."""
    nb = load_nanoback()
    configs = []
    for tk in tickers:
        for combo in itertools.product(*values_per_axis):
            c = nb.BacktestConfig()
            c.data_path = str(DATA / f"{tk}.csv")
            c.ticker = tk
            c.strategy_name = strategy
            for field, val in zip(axis_fields, combo):
                setattr(c, field, val)
            configs.append(c)

    results = nb.run_batch(configs)
    return pd.DataFrame([
        dict(ticker=r.ticker, p1=r.p1, p2=r.p2, fp=r.fp,
             total_return=r.total_return, sharpe_ratio=r.sharpe_ratio,
             max_drawdown=r.max_drawdown, win_rate=r.win_rate,
             total_trades=r.total_trades, total_bars=r.total_bars,
             elapsed_ms=r.elapsed_ms)
        for r in results
    ])


def _minmax(s):
    """Normalize a series to [0, 1]; degenerate (all-equal) → neutral 0.5."""
    lo, hi = s.min(), s.max()
    if hi - lo < 1e-12:
        return pd.Series(0.5, index=s.index)
    return (s - lo) / (hi - lo)


def _pivot(df, x_field, y_field, value):
    """Long results → 2D grid (rows=y axis, cols=x axis). For 1-D strategies
    (no y axis) returns a single-row frame."""
    if y_field is None:
        row = df.set_index(x_field)[value].sort_index()
        return pd.DataFrame([row.values], columns=row.index, index=[value])
    return df.pivot_table(index=y_field, columns=x_field, values=value, aggfunc="mean")


def composite_score(d, weights, min_trades):
    """Return `d` with normalized-metric columns and a `composite` column.
    weights = (sharpe, low_drawdown, win, trade_adequacy), renormalized to sum 1."""
    w_sharpe, w_dd, w_win, w_trade = (w / sum(weights) for w in weights)
    d = d.copy()
    d["_n_sharpe"] = _minmax(d["sharpe_ratio"])
    d["_n_dd"] = 1.0 - _minmax(d["max_drawdown"])  # lower drawdown is better
    d["_n_win"] = _minmax(d["win_rate"])
    d["_n_trade"] = (d["total_trades"] / min_trades).clip(0.0, 1.0)
    d["composite"] = (w_sharpe * d["_n_sharpe"] + w_dd * d["_n_dd"]
                      + w_win * d["_n_win"] + w_trade * d["_n_trade"])
    return d


def _heatmap(piv, title, colorscale, text_fmt, midpoint=None):
    fig = px.imshow(
        piv, text_auto=text_fmt, aspect="auto",
        color_continuous_scale=colorscale, origin="lower",
        labels=dict(color=title),
    )
    if midpoint is not None:
        fig.update_coloraxes(cmid=midpoint)
    fig.update_layout(title=title, margin=dict(l=40, r=20, t=50, b=40), height=420)
    return fig


def main():
    st.set_page_config(page_title="Nanoback Sweep", layout="wide")
    st.title("Nanoback — Parallel Parameter Sweep")

    try:
        nb = load_nanoback()
    except Exception as e:  # noqa: BLE001 - surface any import failure to the user
        st.error(
            f"Could not import `nanoback` from `{BUILD}`.\n\n"
            f"Build the module first (see docs/steps/step_3.md), then reload.\n\n`{e}`"
        )
        st.stop()

    if not hasattr(nb, "run_batch"):
        st.error(
            "The loaded `nanoback` module has no `run_batch` — it is likely a stale "
            "`.so` built for a different Python. Rebuild it under this interpreter."
        )
        st.stop()

    tickers_all = sorted(p.stem for p in DATA.glob("*.csv"))
    if not tickers_all:
        st.warning(f"No CSV datasets found in `{DATA}`.")
        st.stop()

    # ── Sidebar: sweep definition ────────────────────────────────────────────
    with st.sidebar:
        st.header("Sweep setup")
        strategy = st.selectbox("Strategy", list(STRATS.keys()))
        tickers = st.multiselect("Tickers", tickers_all, default=tickers_all[:1])

        axis_inputs = []  # (field, label, kind, start, stop, step)
        for field, label, kind, d_start, d_stop, d_step in STRATS[strategy]:
            st.markdown(f"**{label}** &nbsp;`{field}`")
            c1, c2, c3 = st.columns(3)
            if kind == "int":
                start = c1.number_input("start", value=int(d_start), step=1, key=f"{field}_s")
                stop = c2.number_input("stop", value=int(d_stop), step=1, key=f"{field}_e")
                step = c3.number_input("step", value=int(d_step), min_value=1, step=1, key=f"{field}_t")
            else:
                start = c1.number_input("start", value=float(d_start), key=f"{field}_s")
                stop = c2.number_input("stop", value=float(d_stop), key=f"{field}_e")
                step = c3.number_input("step", value=float(d_step), min_value=0.0001,
                                       format="%.4f", key=f"{field}_t")
            axis_inputs.append((field, label, kind, start, stop, step))

        run = st.button("Run sweep", type="primary")

    axis_fields = tuple(a[0] for a in axis_inputs)
    axis_labels = {a[0]: a[1] for a in axis_inputs}
    values_per_axis = tuple(tuple(axis_values(a[2], a[3], a[4], a[5])) for a in axis_inputs)
    n_combos = len(tickers) * int(np.prod([len(v) for v in values_per_axis]) if values_per_axis else 0)
    st.caption(
        f"**{n_combos}** backtests — {len(tickers)} ticker(s) × "
        + " × ".join(f"{len(v)} {axis_labels[f]}" for f, v in zip(axis_fields, values_per_axis))
    )

    if run:
        if not tickers:
            st.warning("Select at least one ticker.")
            st.stop()
        with st.spinner(f"Running {n_combos} backtests in parallel…"):
            df = run_sweep(strategy, tuple(tickers), axis_fields, values_per_axis)
        st.session_state["sweep"] = dict(df=df, strategy=strategy, axis_fields=axis_fields,
                                         axis_labels=axis_labels)

    sweep = st.session_state.get("sweep")
    if not sweep:
        st.info("Define a grid in the sidebar and click **Run sweep**.")
        st.stop()

    df = sweep["df"]
    axis_fields = sweep["axis_fields"]
    axis_labels = sweep["axis_labels"]
    x_field = axis_fields[0]
    y_field = axis_fields[1] if len(axis_fields) > 1 else None

    # ── Ticker focus ─────────────────────────────────────────────────────────
    tickers_in_result = sorted(df["ticker"].unique())
    focus = tickers_in_result[0]
    if len(tickers_in_result) > 1:
        focus = st.radio("Ticker", tickers_in_result, horizontal=True)
    d = df[df["ticker"] == focus].copy()

    xlab = axis_labels[x_field]
    ylab = axis_labels[y_field] if y_field else ""

    tab_sharpe, tab_win, tab_dd, tab_comp, tab_table = st.tabs(
        ["Sharpe", "Win rate", "Max drawdown", "Composite", "Raw table"]
    )

    with tab_sharpe:
        piv = _pivot(d, x_field, y_field, "sharpe_ratio")
        st.plotly_chart(_heatmap(piv, f"Sharpe — {focus}", "RdYlGn", ".2f", midpoint=0.0),
                        use_container_width=True)
        st.caption(f"x = {xlab} (`{x_field}`)" + (f", y = {ylab} (`{y_field}`)" if y_field else ""))

    with tab_win:
        piv = _pivot(d, x_field, y_field, "win_rate")
        st.plotly_chart(_heatmap(piv, f"Win rate % — {focus}", "Greens", ".1f"),
                        use_container_width=True)

    with tab_dd:
        piv = _pivot(d, x_field, y_field, "max_drawdown")
        # drawdown is a positive % where bigger = worse, so reverse the scale.
        st.plotly_chart(_heatmap(piv, f"Max drawdown % — {focus}", "RdYlGn_r", ".1f"),
                        use_container_width=True)

    with tab_comp:
        st.markdown("**Composite score** — a senior-analyst ranking. Adjust the weights; "
                    "they are renormalized to sum to 1.")
        c1, c2, c3, c4 = st.columns(4)
        w_sharpe = c1.slider("Sharpe", 0.0, 1.0, 0.45, 0.05)
        w_dd = c2.slider("Low drawdown", 0.0, 1.0, 0.25, 0.05)
        w_win = c3.slider("Win rate", 0.0, 1.0, 0.15, 0.05)
        w_trade = c4.slider("Trade adequacy", 0.0, 1.0, 0.15, 0.05)
        min_trades = st.slider("Min trades for full adequacy", 1, 200, 30, 1)

        if (w_sharpe + w_dd + w_win + w_trade) <= 0:
            st.warning("Set at least one weight above zero.")
            st.stop()

        d = composite_score(d, (w_sharpe, w_dd, w_win, w_trade), min_trades)
        piv = _pivot(d, x_field, y_field, "composite")
        fig = _heatmap(piv, f"Composite — {focus}", "Viridis", ".2f")

        # Outline the winning cell.
        best = d.loc[d["composite"].idxmax()]
        cols, rows = list(piv.columns), list(piv.index)
        ci = cols.index(best[x_field])
        ri = rows.index(best[y_field]) if y_field else 0
        fig.add_shape(type="rect", x0=ci - 0.5, x1=ci + 0.5, y0=ri - 0.5, y1=ri + 0.5,
                      line=dict(color="cyan", width=3))
        st.plotly_chart(fig, use_container_width=True)

        winner = f"{xlab}={best[x_field]:g}" + (f", {ylab}={best[y_field]:g}" if y_field else "")
        st.success(
            f"**Best: {focus} · {winner}** — composite {best['composite']:.3f} "
            f"(Sharpe {best['sharpe_ratio']:.2f}, drawdown {best['max_drawdown']:.1f}%, "
            f"win {best['win_rate']:.1f}%, {int(best['total_trades'])} trades)"
        )

    with tab_table:
        cols = ["ticker", x_field] + ([y_field] if y_field else []) + [
            "total_return", "sharpe_ratio", "max_drawdown", "win_rate",
            "total_trades", "elapsed_ms",
        ]
        st.dataframe(df[cols].sort_values("sharpe_ratio", ascending=False),
                     use_container_width=True, hide_index=True)


if __name__ == "__main__":
    main()
