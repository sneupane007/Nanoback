#include "include/strategy/strategy_factory.hpp"
#include "include/strategy/strategy.hpp"
#include "include/strategy/rsi_strategy.hpp"
#include "include/strategy/mean_reversion_strategy.hpp"
#include "include/strategy/momentum_strategy.hpp"
#include "include/strategy/scalping_strategy.hpp"
#include "include/strategy/breakout_strategy.hpp"
#include "include/strategy/pullback_strategy.hpp"
#include "include/strategy/vwap_strategy.hpp"
#include "include/strategy/orb_strategy.hpp"
#include <stdexcept>

std::unique_ptr<IStrategy> make_strategy(const std::string& name,
                                          int param1, int param2, double fparam) {
    if (name == "sma") {
        int fast = (param1 > 0) ? param1 : 10;
        int slow = (param2 > 0) ? param2 : 30;
        return std::make_unique<SmaCrossStrategy>(fast, slow);
    }
    if (name == "rsi") {
        int    period     = (param1 > 0)   ? param1 : 14;
        double overbought = (fparam > 0.0) ? fparam : 70.0;
        return std::make_unique<RsiStrategy>(period, 30.0, overbought);
    }
    if (name == "mean_reversion") {
        int    period = (param1 > 0)   ? param1 : 20;
        double k      = (fparam > 0.0) ? fparam : 2.0;
        return std::make_unique<MeanReversionStrategy>(period, k);
    }
    if (name == "momentum") {
        int    period    = (param1 > 0)   ? param1 : 10;
        double threshold = (fparam > 0.0) ? fparam : 1.0;
        return std::make_unique<MomentumStrategy>(period, threshold);
    }
    if (name == "scalping") {
        int fast = (param1 > 0) ? param1 : 5;
        int slow = (param2 > 0) ? param2 : 13;
        return std::make_unique<ScalpingStrategy>(fast, slow);
    }
    if (name == "breakout") {
        int period = (param1 > 0) ? param1 : 20;
        return std::make_unique<BreakoutStrategy>(period);
    }
    if (name == "pullback") {
        int trend    = (param1 > 0) ? param1 : 50;
        int pullback = (param2 > 0) ? param2 : 10;
        return std::make_unique<PullbackStrategy>(trend, pullback);
    }
    if (name == "vwap") {
        int period = (param1 > 0) ? param1 : 20;
        return std::make_unique<VwapStrategy>(period);
    }
    if (name == "orb") {
        int range_bars = (param1 > 0) ? param1 : 5;
        int cycle_bars = (param2 > 0) ? param2 : 30;
        return std::make_unique<OrbStrategy>(range_bars, cycle_bars);
    }
    throw std::invalid_argument("Unknown strategy: '" + name +
                                "'. Valid options: sma, rsi, mean_reversion, "
                                "momentum, scalping, breakout, pullback, vwap, orb");
}
