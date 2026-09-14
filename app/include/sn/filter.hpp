/**
 * @file filter.hpp
 * @brief Integer signal conditioning: median-of-3 spike rejection, EMA, window statistics.
 *
 * All state is fixed-size and allocation-free; arithmetic is integer-only so
 * results are bit-identical on target and host.
 */
#ifndef SN_FILTER_HPP
#define SN_FILTER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace sn {

/** Integer square root (floor). */
std::uint32_t isqrt64(std::uint64_t value);

/** Median of the last three samples; passes the first samples through. */
class Median3 {
public:
    /** Push a sample and return the current median. */
    std::int32_t push(std::int32_t value);

private:
    std::array<std::int32_t, 3> buf_{};
    std::uint8_t count_{0U};
    std::uint8_t next_{0U};
};

/** Exponential moving average y += (x - y) / 2^shift in Q8 fixed point. */
class Ema {
public:
    /** @param shift Smoothing exponent (1..15); larger is smoother. */
    explicit Ema(std::uint8_t shift);
    /** Push a sample and return the rounded average. */
    std::int32_t push(std::int32_t value);

private:
    std::int64_t state_q8_{0};
    std::uint8_t shift_;
    bool primed_{false};
};

/** Spike rejection followed by smoothing. */
class ChannelFilter {
public:
    /** @param ema_shift See Ema. */
    explicit ChannelFilter(std::uint8_t ema_shift) : ema_(ema_shift) {}
    /** Filter one sample. */
    std::int32_t push(std::int32_t value) { return ema_.push(median_.push(value)); }

private:
    Median3 median_;
    Ema ema_;
};

/** Sliding-window min/max/mean/standard deviation over the last N samples. */
template <std::size_t N>
class WindowStats {
    static_assert(N >= 2U, "window must hold at least two samples");

public:
    /** Add a sample, evicting the oldest when full. */
    void push(std::int32_t value)
    {
        if (count_ == N) {
            const std::int64_t old = buf_[head_];
            sum_ -= old;
            sum_sq_ -= old * old;
        } else {
            ++count_;
        }
        buf_[head_] = value;
        sum_ += value;
        sum_sq_ += static_cast<std::int64_t>(value) * value;
        head_ = (head_ + 1U) % N;
    }

    /** Samples currently held. */
    std::size_t count() const { return count_; }

    /** Minimum (0 when empty). */
    std::int32_t min() const
    {
        std::int32_t m = (count_ > 0U) ? buf_[0] : 0;
        for (std::size_t i = 1U; i < count_; ++i) {
            m = (buf_[i] < m) ? buf_[i] : m;
        }
        return m;
    }

    /** Maximum (0 when empty). */
    std::int32_t max() const
    {
        std::int32_t m = (count_ > 0U) ? buf_[0] : 0;
        for (std::size_t i = 1U; i < count_; ++i) {
            m = (buf_[i] > m) ? buf_[i] : m;
        }
        return m;
    }

    /** Rounded mean (0 when empty). */
    std::int32_t mean() const
    {
        if (count_ == 0U) {
            return 0;
        }
        const auto n = static_cast<std::int64_t>(count_);
        const std::int64_t half = (sum_ >= 0) ? (n / 2) : -(n / 2);
        return static_cast<std::int32_t>((sum_ + half) / n);
    }

    /** Population standard deviation (floor). */
    std::uint32_t stddev() const
    {
        if (count_ < 2U) {
            return 0U;
        }
        const auto n = static_cast<std::int64_t>(count_);
        const std::int64_t num = (n * sum_sq_) - (sum_ * sum_);
        if (num <= 0) {
            return 0U;
        }
        return isqrt64(static_cast<std::uint64_t>(num) / static_cast<std::uint64_t>(n * n));
    }

private:
    std::array<std::int32_t, N> buf_{};
    std::size_t head_{0U};
    std::size_t count_{0U};
    std::int64_t sum_{0};
    std::int64_t sum_sq_{0};
};

}  // namespace sn

#endif  // SN_FILTER_HPP
