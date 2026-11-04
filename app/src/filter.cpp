/**
 * @file filter.cpp
 * @brief Filter implementations.
 */
#include "sn/filter.hpp"

namespace sn {

std::uint32_t isqrt64(std::uint64_t value)
{
    std::uint64_t result = 0U;
    std::uint64_t bit = 1ULL << 62;
    while (bit > value) {
        bit >>= 2;
    }
    std::uint64_t rem = value;
    while (bit != 0U) {
        if (rem >= (result + bit)) {
            rem -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<std::uint32_t>(result);
}

std::int32_t Median3::push(std::int32_t value)
{
    buf_[next_] = value;
    next_ = static_cast<std::uint8_t>((next_ + 1U) % 3U);
    if (count_ < 3U) {
        ++count_;
        if (count_ < 3U) {
            return value;
        }
    }
    const std::int32_t a = buf_[0];
    const std::int32_t b = buf_[1];
    const std::int32_t c = buf_[2];
    const std::int32_t lo = (a < b) ? a : b;
    const std::int32_t hi = (a < b) ? b : a;
    const std::int32_t upper = (hi < c) ? hi : c;
    return (lo > upper) ? lo : upper;
}

namespace {
std::uint8_t clamp_shift(std::uint8_t shift)
{
    if (shift == 0U) {
        return 1U;
    }
    return (shift > 15U) ? static_cast<std::uint8_t>(15U) : shift;
}
}  // namespace

Ema::Ema(std::uint8_t shift) : shift_(clamp_shift(shift)) {}

std::int32_t Ema::push(std::int32_t value)
{
    const std::int64_t x = static_cast<std::int64_t>(value) * 256;
    if (!primed_) {
        state_q8_ = x;
        primed_ = true;
    } else {
        const std::int64_t delta = x - state_q8_;
        const std::int64_t div = static_cast<std::int64_t>(1) << shift_;
        state_q8_ += delta / div;
    }
    const std::int64_t rounded = (state_q8_ >= 0) ? (state_q8_ + 128) : (state_q8_ - 128);
    return static_cast<std::int32_t>(rounded / 256);
}

}  // namespace sn
