#include <iostream>
#include <string>

#include "include/events/events.hpp"
#include "include/events/event_queue.hpp"
#include "include/data/data_handler.hpp"
#include "include/strategy/strategy.hpp"
#include "include/portfolio/portfolio.hpp"
#include "include/execution/execution_handler.hpp"
#include "include/performance/performance.hpp"
#include "include/utils/overload.hpp"
#include "include/utils/benchmark.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <csv_path> <ticker>\n";
        return 1;
    }

    const std::string csv_path = argv[1];
    const std::string ticker   = argv[2];

    // --- Component construction (heap allocation happens here, not in the loop) ---
    EventQueue         queue;
    DataHandler        data(csv_path, ticker);
    SmaCrossStrategy   strategy;
    Portfolio          portfolio(100000.0);
    ExecutionHandler   execution;
    PerformanceTracker perf(10000);

    portfolio.add_ticker(ticker);

    // Flat price array indexed by ticker id (id=0 for the single asset).
    double prices[Portfolio::MAX_ASSETS] = {};

    double  latest_close = 0.0;
    int64_t total_ns     = 0;
    uint64_t event_count = 0;

    // --- Outer loop: one bar per iteration ---
    while (data.stream_next_event(queue)) {

        // --- Inner loop: drain the queue fully before reading the next bar ---
        while (!queue.empty()) {
            Event e = queue.pop();

            auto t0 = Benchmark::now();

            std::visit(overload{
                [&](const MarketEvent& m) {
                    latest_close = m.close;
                    prices[0]    = m.close;           // id 0 = this ticker
                    strategy.on_market_event(m, queue);
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

            total_ns += Benchmark::elapsed_ns(t0);
            ++event_count;
        }

        // Record equity once per bar after the full event cascade for this bar is drained.
        // This ensures one snapshot per bar — prevents Sharpe ratio dilution from
        // multiple intra-bar snapshots (pre-fill and post-fill at the same timestamp).
        perf.record_equity(portfolio.equity(prices));
    }

    perf.report();

    std::cout << "\n=== Timing ===\n";
    std::cout << "Events processed : " << event_count << "\n";
    std::cout << "Total time       : " << total_ns    << " ns\n";
    std::cout << "Avg per event    : "
              << (event_count ? total_ns / (int64_t)event_count : 0)
              << " ns\n";

    return 0;
}
