/**
 * @file command.hpp
 * @brief Host command handling, decoupled from the RTOS through NodeControl.
 */
#ifndef SN_COMMAND_HPP
#define SN_COMMAND_HPP

#include <cstddef>
#include <cstdint>

#include "sn/protocol.hpp"

namespace sn {

/** Side effects a command may request. */
class NodeControl {
public:
    /** Apply a new sampling period (already range-checked). */
    virtual void set_period_ms(std::uint16_t period_ms) = 0;
    /** Schedule a software reset after the reply is sent. */
    virtual void request_reboot() = 0;
    /** Schedule a reset into the bootloader after the reply is sent. */
    virtual void request_bootloader() = 0;
    /** Running firmware identity. */
    virtual proto::Version version() const = 0;

protected:
    NodeControl() = default;
    NodeControl(const NodeControl&) = default;
    NodeControl(NodeControl&&) = default;
    NodeControl& operator=(const NodeControl&) = default;
    NodeControl& operator=(NodeControl&&) = default;
    ~NodeControl() = default;
};

/**
 * @brief Execute one decoded command payload.
 * @return Reply length written to @p resp (0 if nothing to send).
 */
std::size_t handle_command(const std::uint8_t* req, std::size_t len, NodeControl& node, std::uint8_t* resp,
                           std::size_t cap);

}  // namespace sn

#endif  // SN_COMMAND_HPP
