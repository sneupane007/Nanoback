#include <pybind11/pybind11.h>
#include <pybind11/stl.h>   // required for std::vector <-> Python list conversion

#include "include/run_backtest.hpp"

namespace py = pybind11;

// One high-level module: the caller builds BacktestConfig objects, hands a list
// of them to run_batch(), and reads back plain SweepResult records. Individual
// engine classes (EventQueue, Portfolio, ...) are intentionally NOT exposed.
PYBIND11_MODULE(nanoback, m) {
    m.doc() = "Nanoback backtesting engine — bindings for parallel parameter sweeps.";

    py::class_<BacktestConfig>(m, "BacktestConfig")
        .def(py::init<>())
        .def_readwrite("data_path",       &BacktestConfig::data_path)
        .def_readwrite("ticker",          &BacktestConfig::ticker)
        .def_readwrite("strategy_name",   &BacktestConfig::strategy_name)
        .def_readwrite("p1",              &BacktestConfig::p1)
        .def_readwrite("p2",              &BacktestConfig::p2)
        .def_readwrite("fp",              &BacktestConfig::fp)
        .def_readwrite("use_proto",       &BacktestConfig::use_proto)
        .def_readwrite("initial_capital", &BacktestConfig::initial_capital);

    py::class_<SweepResult>(m, "SweepResult")
        .def_readonly("ticker",       &SweepResult::ticker)
        .def_readonly("p1",           &SweepResult::p1)
        .def_readonly("p2",           &SweepResult::p2)
        .def_readonly("fp",           &SweepResult::fp)
        .def_readonly("total_return", &SweepResult::total_return)
        .def_readonly("sharpe_ratio", &SweepResult::sharpe_ratio)
        .def_readonly("max_drawdown", &SweepResult::max_drawdown)
        .def_readonly("win_rate",     &SweepResult::win_rate)
        .def_readonly("total_trades", &SweepResult::total_trades)
        .def_readonly("total_bars",   &SweepResult::total_bars)
        .def_readonly("elapsed_ms",   &SweepResult::elapsed_ms);

    // BacktestResult is exposed with only the fields run_backtest() actually
    // populates (per-bar equity, per-trade pnls, and scalar metrics/throughput).
    // market_data/signals/timestamps are left unbound — the loop doesn't collect
    // them, so exposing empty vectors would be misleading.
    py::class_<BacktestResult>(m, "BacktestResult")
        .def_readonly("equity_curve",     &BacktestResult::equity_curve)
        .def_readonly("trade_pnls",       &BacktestResult::trade_pnls)
        .def_readonly("total_return",     &BacktestResult::total_return)
        .def_readonly("sharpe_ratio",     &BacktestResult::sharpe_ratio)
        .def_readonly("max_drawdown",     &BacktestResult::max_drawdown)
        .def_readonly("win_rate",         &BacktestResult::win_rate)
        .def_readonly("total_trades",     &BacktestResult::total_trades)
        .def_readonly("total_bars",       &BacktestResult::total_bars)
        .def_readonly("elapsed_ms",       &BacktestResult::elapsed_ms)
        .def_readonly("parse_ms",         &BacktestResult::parse_ms)
        .def_readonly("process_ms",       &BacktestResult::process_ms)
        .def_readonly("total_events",     &BacktestResult::total_events)
        .def_readonly("peak_queue_depth", &BacktestResult::peak_queue_depth)
        .def_readonly("events_per_sec",   &BacktestResult::events_per_sec)
        .def_readonly("bars_per_sec",     &BacktestResult::bars_per_sec)
        .def_readonly("column_align_ms",  &BacktestResult::column_align_ms);

    m.def("run_backtest", &run_backtest, py::arg("config"),
          py::call_guard<py::gil_scoped_release>(),
          "Run a single backtest and return its metrics and series.");

    // GIL is released for the whole batch: the C++ workers touch no Python
    // objects, so other Python threads (e.g. Streamlit's) keep running while the
    // sweep executes. pybind re-acquires the GIL to convert the returned list.
    m.def("run_batch", &run_batch, py::arg("configs"),
          py::call_guard<py::gil_scoped_release>(),
          "Run many backtests in parallel; returns one SweepResult per config.");
}
