#include "include/run_backtest.hpp"

#include <memory>
#include <variant>
#include <cstdint>

#include "include/events.hpp"
#include "include/event_queue.hpp"
#include "include/idata_handler.hpp"
#include "include/data_handler.hpp"
#include "include/proto_data_handler.hpp"
#include "include/strategy/strategy_factory.hpp"
#include "include/portfolio.hpp"
#include "include/execution_handler.hpp"
#include "include/performance.hpp"
#include "include/overload.hpp"
#include "include/benchmark.hpp"

// Single backtest run — the event loop lifted verbatim from the original
// main.cpp. No stdout: every result is returned in the struct so this is safe
// to run on many threads concurrently.
BacktestResult run_backtest(const BacktestConfig& cfg) {
    // make_strategy throws std::invalid_argument for an unknown name; let it
    // propagate so the caller decides how to surface it.
    std::unique_ptr<IStrategy> strategy =
        make_strategy(cfg.strategy_name, cfg.p1, cfg.p2, cfg.fp);

    EventQueue queue;

    std::unique_ptr<IDataHandler> data;
    double column_align_ms = 0.0;
    if (cfg.use_proto) {
        data = std::make_unique<ProtoDataHandler>(cfg.data_path, cfg.ticker);
    } else {
        // header_align_ms() is a DataHandler-only cost (CSV column detection),
        // so capture it from the concrete type before erasing to the interface.
        auto csv = std::make_unique<DataHandler>(cfg.data_path, cfg.ticker);
        column_align_ms = csv->header_align_ms();
        data = std::move(csv);
    }

    Portfolio          portfolio(cfg.initial_capital);
    ExecutionHandler   execution;
    PerformanceTracker perf(10000);

    portfolio.add_ticker(cfg.ticker);

    double prices[Portfolio::MAX_ASSETS] = {};

    double   latest_close = 0.0;
    uint64_t event_count  = 0;
    uint64_t bar_count    = 0;
    int64_t  parse_ns     = 0;   // time inside the data handler: read + parse
    int64_t  process_ns   = 0;   // time inside the event-dispatch cascade

    const auto wall_t0 = Benchmark::now();

    while (true) {
        const auto tp  = Benchmark::now();
        const bool got = data->stream_next_event(queue);
        parse_ns += Benchmark::elapsed_ns(tp);
        if (!got) break;
        ++bar_count;

        const auto tc = Benchmark::now();
        while (!queue.empty()) {
            Event e = queue.pop();

            std::visit(overload{
                [&](const MarketEvent& m) {
                    latest_close = m.close;
                    prices[0]    = m.close;
                    strategy->on_market_event(m, queue);
                },
                [&](const SignalEvent& s) {
                    portfolio.on_signal(s, queue, latest_close);
                },
                [&](const OrderEvent& o) {
                    execution.on_order(o, queue, latest_close);
                },
                [&](const FillEvent& f) {
                    double realized_pnl = portfolio.on_fill(f);
                    if (f.direction == OrderDirection::EXIT) {
                        perf.record_trade(realized_pnl);
                    }
                },
            }, e);

            ++event_count;
        }
        process_ns += Benchmark::elapsed_ns(tc);

        perf.record_equity(portfolio.equity(prices));
    }

    const int64_t wall_ns   = Benchmark::elapsed_ns(wall_t0);
    const double  wall_secs = wall_ns / 1e9;

    BacktestResult r;
    r.equity_curve     = perf.equity_curve();   // copy for programmatic access
    r.trade_pnls       = perf.trade_pnls();
    r.total_return     = perf.total_return();
    r.sharpe_ratio     = perf.annualised_sharpe();
    r.max_drawdown     = perf.max_drawdown();
    r.win_rate         = perf.win_rate();
    r.total_trades     = static_cast<int>(perf.trade_pnls().size());
    r.total_bars       = static_cast<int>(bar_count);
    r.elapsed_ms       = wall_ns    / 1e6;
    r.parse_ms         = parse_ns   / 1e6;
    r.process_ms       = process_ns / 1e6;
    r.total_events     = event_count;
    r.peak_queue_depth = queue.peak_depth();
    r.events_per_sec   = wall_secs > 0.0 ? event_count / wall_secs : 0.0;
    r.bars_per_sec     = wall_secs > 0.0 ? bar_count   / wall_secs : 0.0;
    r.column_align_ms  = column_align_ms;
    return r;
}
