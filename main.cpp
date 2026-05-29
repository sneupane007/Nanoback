#include <iostream>
#include <memory>
#include <string>

#include "include/events.hpp"
#include "include/event_queue.hpp"
#include "include/data_handler.hpp"
#include "include/strategy/strategy_factory.hpp"
#include "include/portfolio.hpp"
#include "include/execution_handler.hpp"
#include "include/performance.hpp"
#include "include/overload.hpp"
#include "include/benchmark.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <csv_path> <ticker>"
                  << " [--strategy sma|rsi|mean_reversion]"
                  << " [--p1 <int>] [--p2 <int>] [--fp <double>]\n";
        return 1;
    }

    const std::string csv_path = argv[1];
    const std::string ticker   = argv[2];

    // --- Parse strategy flags ---
    std::string strategy_name = "sma"; // default
    int    p1 = 0;
    int    p2 = 0;
    double fp = 0.0;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--strategy" && i + 1 < argc) strategy_name = argv[++i];
        else if (arg == "--p1"       && i + 1 < argc) p1  = std::stoi(argv[++i]);
        else if (arg == "--p2"       && i + 1 < argc) p2  = std::stoi(argv[++i]);
        else if (arg == "--fp"       && i + 1 < argc) fp  = std::stod(argv[++i]);
    }

    std::unique_ptr<IStrategy> strategy;
    try {
        strategy = make_strategy(strategy_name, p1, p2, fp);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Strategy: " << strategy_name << "\n";
    std::cout << "sizeof(MarketEvent)=" << sizeof(MarketEvent)
              << "  sizeof(Event)=" << sizeof(Event)
              << "  buffer_bytes=" << sizeof(Event) * 1024 << "\n\n";

    EventQueue         queue;
    DataHandler        data(csv_path, ticker);
    Portfolio          portfolio(100000.0);
    ExecutionHandler   execution;
    PerformanceTracker perf(10000);

    portfolio.add_ticker(ticker);

   
    double prices[Portfolio::MAX_ASSETS] = {};

    double  latest_close = 0.0;
    int64_t total_ns     = 0;
    uint64_t event_count = 0;

    while (data.stream_next_event(queue)) {

        
        while (!queue.empty()) {
            Event e = queue.pop();

            auto t0 = Benchmark::now();

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

            total_ns += Benchmark::elapsed_ns(t0);
            ++event_count;
        }

        
        perf.record_equity(portfolio.equity(prices));
    }

    perf.report();

    std::cout << "\n=== Timing ===\n";
    std::cout << "Events processed : " << event_count << "\n";
    std::cout << "Peak queue depth : " << queue.peak_depth() << "\n";
    std::cout << "Total time       : " << total_ns    << " ns\n";
    std::cout << "Avg per event    : "
              << (event_count ? total_ns / (int64_t)event_count : 0)
              << " ns\n";

    return 0;
}
