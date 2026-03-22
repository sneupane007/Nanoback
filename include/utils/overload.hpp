#pragma once

// C++17 overload pattern for std::visit on std::variant.
// Usage: std::visit(overload{ [](TypeA& a){...}, [](TypeB& b){...} }, variant);
template<typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};

// C++17 deduction guide — lets the compiler infer template args from the lambdas.
template<typename... Ts>
overload(Ts...) -> overload<Ts...>;
