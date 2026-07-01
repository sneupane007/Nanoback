#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "include/run_backtest.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Writes a small, valid OHLCV CSV to /tmp so the tests don't depend on the
// process working directory (ctest may run from anywhere). 60 bars is enough
// warm-up for an sma(3,5) cross to emit signals. The close oscillates so the
// crossover actually triggers trades.
std::string write_temp_csv(const std::string& path =
                               "/tmp/nanoback_run_backtest_test.csv") {
    std::ofstream f(path);
    f << "date,open,high,low,close,volume\n";
    for (int i = 0; i < 60; ++i) {
        double c = 100.0 + (i % 10) - 5;  // 95..104, oscillating
        f << "2024-01-02," << c << "," << (c + 1) << "," << (c - 1) << ","
          << c << ",1000\n";
    }
    return path;
}

BacktestConfig base_cfg(const std::string& path) {
    BacktestConfig cfg;
    cfg.data_path     = path;
    cfg.ticker        = "TEST";
    cfg.strategy_name = "sma";
    cfg.p1            = 3;
    cfg.p2            = 5;
    return cfg;
}

}  // namespace

TEST_CASE("run_backtest returns a populated result for a CSV") {
    const std::string path = write_temp_csv();
    BacktestResult r = run_backtest(base_cfg(path));

    CHECK(r.total_bars == 60);
    CHECK(r.equity_curve.size() == 60);
    CHECK(r.peak_queue_depth >= 1);
}

TEST_CASE("run_batch of N identical configs returns N identical SweepResults") {
    const std::string path = write_temp_csv();
    std::vector<BacktestConfig> configs(16, base_cfg(path));

    std::vector<SweepResult> out = run_batch(configs);
    REQUIRE(out.size() == 16);

    // Determinism + thread isolation: every parallel run must match the first.
    for (const auto& s : out) {
        CHECK(s.ticker == "TEST");
        CHECK(s.total_bars   == out[0].total_bars);
        CHECK(s.total_trades == out[0].total_trades);
        CHECK(s.total_return == doctest::Approx(out[0].total_return));
        CHECK(s.sharpe_ratio == doctest::Approx(out[0].sharpe_ratio));
        CHECK(s.max_drawdown == doctest::Approx(out[0].max_drawdown));
        CHECK(s.win_rate     == doctest::Approx(out[0].win_rate));
    }
}

TEST_CASE("run_batch echoes each config's sweep axes in input order") {
    const std::string path = write_temp_csv();
    std::vector<BacktestConfig> configs;
    for (int p1 = 2; p1 <= 6; ++p1) {  // spans more than one wave on low-core CI
        BacktestConfig cfg = base_cfg(path);
        cfg.p1 = p1;
        cfg.p2 = 10;
        configs.push_back(cfg);
    }

    std::vector<SweepResult> out = run_batch(configs);
    REQUIRE(out.size() == configs.size());
    for (size_t i = 0; i < out.size(); ++i) {
        CHECK(out[i].p1 == configs[i].p1);
        CHECK(out[i].p2 == 10);
    }
}

TEST_CASE("run_batch validates strategy names up front") {
    const std::string path = write_temp_csv();
    BacktestConfig cfg = base_cfg(path);
    cfg.strategy_name = "does_not_exist";
    std::vector<BacktestConfig> configs{cfg};

    CHECK_THROWS_AS(run_batch(configs), std::invalid_argument);
}

TEST_CASE("run_batch on empty input returns empty") {
    CHECK(run_batch({}).empty());
}
