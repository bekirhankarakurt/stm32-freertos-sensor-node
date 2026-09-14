/**
 * @file health.hpp
 * @brief Task-aware watchdog supervisor and update confirmation policy.
 *
 * The hardware watchdog is fed only while every supervised task has checked
 * in within its deadline, so a single stalled task resets the node even
 * though the feeding task itself still runs.
 */
#ifndef SN_HEALTH_HPP
#define SN_HEALTH_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace sn {

/** Supervised tasks. */
enum class TaskId : std::uint8_t { kSampler = 0, kFilter, kTelemetry, kCommand, kCount };

/** Result of one supervision cycle. */
struct HealthVerdict {
    bool feed;    /**< Refresh the hardware watchdog. */
    bool confirm; /**< Mark the running image as good (once). */
};

/** Liveness supervisor. */
class HealthMonitor {
public:
    /**
     * @param deadline_ms   Maximum age of a task check-in.
     * @param confirm_after_ms Continuous healthy time before confirming an image.
     */
    HealthMonitor(std::uint32_t deadline_ms, std::uint32_t confirm_after_ms);

    /** Record that @p id made progress at @p now_ms (any context). */
    void checkin(TaskId id, std::uint32_t now_ms);

    /** True when every task checked in within the deadline. */
    bool all_alive(std::uint32_t now_ms) const;

    /** Run one supervision cycle. */
    HealthVerdict evaluate(std::uint32_t now_ms);

    /** Cycles in which a task was late. */
    std::uint32_t late_cycles() const { return late_cycles_; }

private:
    static constexpr std::size_t kTasks = static_cast<std::size_t>(TaskId::kCount);
    std::array<std::atomic<std::uint32_t>, kTasks> last_{};
    std::uint32_t deadline_ms_;
    std::uint32_t confirm_after_ms_;
    std::uint32_t healthy_since_{0U};
    bool healthy_{false};
    bool confirmed_{false};
    std::uint32_t late_cycles_{0U};
};

}  // namespace sn

#endif  // SN_HEALTH_HPP
