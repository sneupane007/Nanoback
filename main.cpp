#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>

#include "include/events.hpp"        // sizeof(MarketEvent)/sizeof(Event) banner
#include "include/run_backtest.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <data_path> <ticker>"
                  << " [--proto]"
                  << " [--strategy sma|rsi|mean_reversion|momentum|scalping|breakout|pullback|vwap|orb]"
                  << " [--p1 <int>] [--p2 <int>] [--fp <double>]\n";
        return 1;
    }

    // --- Parse args into a BacktestConfig ---
    BacktestConfig cfg;
    cfg.data_path = argv[1];
    cfg.ticker    = argv[2];

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--proto")                    cfg.use_proto     = true;
        else if (arg == "--strategy" && i + 1 < argc) cfg.strategy_name = argv[++i];
        else if (arg == "--p1"       && i + 1 < argc) cfg.p1            = std::stoi(argv[++i]);
        else if (arg == "--p2"       && i + 1 < argc) cfg.p2            = std::stoi(argv[++i]);
        else if (arg == "--fp"       && i + 1 < argc) cfg.fp            = std::stod(argv[++i]);
    }

    // run_backtest() constructs the strategy internally and throws for an
    // unknown name — it runs silently, so on error nothing has been printed yet.
    BacktestResult r;
    try {
        r = run_backtest(cfg);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Strategy: " << cfg.strategy_name << "\n";
    std::cout << "Data source: " << (cfg.use_proto ? "protobuf" : "CSV") << "\n";
    std::cout << "sizeof(MarketEvent)=" << sizeof(MarketEvent)
              << "  sizeof(Event)=" << sizeof(Event)
              << "  buffer_bytes=" << sizeof(Event) * 1024 << "\n\n";

    // --- Performance report (same layout PerformanceTracker::report() prints) ---
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n=== Backtest Performance Report ===\n";
    std::cout << "Total Return : " << r.total_return << "%\n";
    std::cout << std::setprecision(4);
    std::cout << "Sharpe Ratio : " << r.sharpe_ratio << "\n";
    std::cout << std::setprecision(2);
    std::cout << "Max Drawdown : " << r.max_drawdown << "%\n";
    std::cout << "Win Rate     : " << r.win_rate     << "%\n";
    std::cout << "Total Trades : " << r.total_trades << "\n";
    std::cout << "Total Bars   : " << r.total_bars   << "\n";

    // --- Timing (inherits the fixed/precision(2) stream state set above) ---
    auto kbars_s = [&](double ms) { return ms > 0.0 ? r.total_bars / ms : 0.0; };

    std::cout << "\n=== Timing ===\n";
    std::cout << "Bars (MarketEvents) : " << r.total_bars       << "\n";
    std::cout << "Events processed    : " << r.total_events     << "\n";
    std::cout << "Peak queue depth    : " << r.peak_queue_depth << "\n";
    std::cout << "Parse  (data ingest): " << r.parse_ms   << " ms  ("
              << kbars_s(r.parse_ms)   << " k bars/s)\n";
    std::cout << "Process (cascade)   : " << r.process_ms << " ms  ("
              << kbars_s(r.process_ms) << " k bars/s)\n";
    std::cout << "Wall clock          : " << r.elapsed_ms << " ms  ("
              << kbars_s(r.elapsed_ms) << " k bars/s)\n";
    std::cout << "Parse share of wall : "
              << (r.elapsed_ms > 0.0 ? 100.0 * r.parse_ms / r.elapsed_ms : 0.0) << " %\n";

    return 0;
}
