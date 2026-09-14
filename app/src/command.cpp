/**
 * @file command.cpp
 * @brief Host command dispatcher.
 */
#include "sn/command.hpp"

#include "sn/le.h"

namespace sn {

std::size_t handle_command(const std::uint8_t* req, std::size_t len, NodeControl& node, std::uint8_t* resp,
                           std::size_t cap)
{
    if (len < 2U) {
        return 0U;
    }
    const std::uint8_t type = req[0];
    const std::uint8_t seq = req[1];

    switch (type) {
    case SN_MSG_CMD_GET_VERSION:
        if (len != 2U) {
            return proto::encode_ack(type, SN_STATUS_BAD_LENGTH, seq, resp, cap);
        }
        return proto::encode_version(node.version(), seq, resp, cap);

    case SN_MSG_CMD_SET_PERIOD: {
        if (len != 4U) {
            return proto::encode_ack(type, SN_STATUS_BAD_LENGTH, seq, resp, cap);
        }
        const std::uint16_t period = sn_le_get16(&req[2]);
        if ((period < proto::kMinPeriodMs) || (period > proto::kMaxPeriodMs)) {
            return proto::encode_ack(type, SN_STATUS_OUT_OF_RANGE, seq, resp, cap);
        }
        node.set_period_ms(period);
        return proto::encode_ack(type, SN_STATUS_OK, seq, resp, cap);
    }

    case SN_MSG_CMD_REBOOT:
        if (len != 2U) {
            return proto::encode_ack(type, SN_STATUS_BAD_LENGTH, seq, resp, cap);
        }
        node.request_reboot();
        return proto::encode_ack(type, SN_STATUS_OK, seq, resp, cap);

    case SN_MSG_CMD_BOOTLOADER:
        if (len != 2U) {
            return proto::encode_ack(type, SN_STATUS_BAD_LENGTH, seq, resp, cap);
        }
        node.request_bootloader();
        return proto::encode_ack(type, SN_STATUS_OK, seq, resp, cap);

    default:
        return proto::encode_ack(type, SN_STATUS_UNKNOWN, seq, resp, cap);
    }
}

}  // namespace sn
