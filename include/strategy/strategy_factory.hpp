#pragma once
#include <memory>
#include <string>
#include "istrategy.hpp"

// Creates the appropriate IStrategy subclass by name.
// Supported names:
//   "sma"            — SMA crossover          (p1=fast, p2=slow)
//   "rsi"            — RSI mean-reversion      (p1=period, fp=overbought)
//   "mean_reversion" — Bollinger Band reversion (p1=period, fp=k)
//   "momentum"       — Rate-of-Change momentum  (p1=period, fp=threshold%)
//   "scalping"       — EMA golden/death cross   (p1=fast, p2=slow)
//   "breakout"       — Donchian channel breakout (p1=period)
//   "pullback"       — Trend + pullback EMA      (p1=trend, p2=pullback)
//   "vwap"           — Rolling VWAP cross        (p1=period)
//   "orb"            — Opening Range Breakout    (p1=range_bars, p2=cycle_bars)
//
// param1  — first integer parameter (period, fast window, etc.)
// param2  — second integer parameter (slow window, cycle bars, etc.)
// fparam  — floating-point parameter (overbought threshold, Bollinger k, momentum %, etc.)
//
// Uses sensible defaults when params are 0 / 0.0.
// Throws std::invalid_argument if name is unrecognised.
std::unique_ptr<IStrategy> make_strategy(const std::string& name,
                                          int    param1 = 0,
                                          int    param2 = 0,
                                          double fparam = 0.0);
