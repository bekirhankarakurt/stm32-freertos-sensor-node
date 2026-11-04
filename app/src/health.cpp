/**
 * @file health.cpp
 * @brief Task-aware watchdog supervisor.
 */
#include "sn/health.hpp"

#include <algorithm>

namespace sn {

HealthMonitor::HealthMonitor(std::uint32_t deadline_ms, std::uint32_t confirm_after_ms)
    : deadline_ms_(deadline_ms), confirm_after_ms_(confirm_after_ms)
{
    for (auto& slot : last_) {
        slot.store(0U, std::memory_order_relaxed);
    }
}

void HealthMonitor::checkin(TaskId id, std::uint32_t now_ms)
{
    const auto index = static_cast<std::size_t>(id);
    if (index < kTasks) {
        last_[index].store(now_ms, std::memory_order_release);
    }
}

bool HealthMonitor::all_alive(std::uint32_t now_ms) const
{
    /* Unsigned subtraction handles tick wrap-around. */
    return std::all_of(last_.begin(), last_.end(), [this, now_ms](const std::atomic<std::uint32_t>& slot) {
        return (now_ms - slot.load(std::memory_order_acquire)) <= deadline_ms_;
    });
}

HealthVerdict HealthMonitor::evaluate(std::uint32_t now_ms)
{
    HealthVerdict v{false, false};
    if (!all_alive(now_ms)) {
        healthy_ = false;
        ++late_cycles_;
        return v;
    }
    v.feed = true;
    if (!healthy_) {
        healthy_ = true;
        healthy_since_ = now_ms;
    }
    if (!confirmed_ && ((now_ms - healthy_since_) >= confirm_after_ms_)) {
        confirmed_ = true;
        v.confirm = true;
    }
    return v;
}

}  // namespace sn
