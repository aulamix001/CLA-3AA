#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Cla {
constexpr std::size_t parameterCount = 9;
enum Index : std::size_t { Gain, Peak, HiFreq, Mix, Trim, Meter, Mode, Analog, Bypass };
struct ParameterSpec { std::uint32_t id; double min, max, initial; };
inline constexpr std::array<ParameterSpec, parameterCount> specs {{
    {100,0,10,5.5}, {101,0,10,4}, {102,0,100,50}, {103,0,100,100},
    {104,-18,18,0}, {105,0,2,1}, {106,0,1,0}, {107,0,2,1}, {108,0,1,0}
}};
using Values = std::array<double, parameterCount>;
inline Values initialValues() {
    Values values{};
    for (std::size_t i=0;i<parameterCount;++i) values[i]=specs[i].initial;
    return values;
}
inline double clampValue(std::size_t i, double value) {
    const auto bounded=std::isfinite(value) ? std::clamp(value,specs[i].min,specs[i].max) : specs[i].initial;
    return i>=Meter ? std::round(bounded) : bounded;
}
inline double normalize(std::size_t i, double value) {
    return (clampValue(i,value)-specs[i].min)/(specs[i].max-specs[i].min);
}
inline double denormalize(std::size_t i,double value) {
    return clampValue(i,specs[i].min+std::clamp(std::isfinite(value)?value:0.,0.,1.)*(specs[i].max-specs[i].min));
}
inline int indexForId(std::uint32_t id) {
    for (std::size_t i=0;i<parameterCount;++i) if(specs[i].id==id) return static_cast<int>(i);
    return -1;
}
}
