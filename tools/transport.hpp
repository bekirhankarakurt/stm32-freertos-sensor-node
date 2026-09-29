/**
 * @file transport.hpp
 * @brief Framed message transport used by the host tools.
 */
#ifndef SN_TOOLS_TRANSPORT_HPP
#define SN_TOOLS_TRANSPORT_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sn::tools {

/** Sends and receives complete payloads (framing handled by the implementation). */
class Transport {
public:
    Transport() = default;
    Transport(const Transport&) = delete;
    Transport(Transport&&) = delete;
    Transport& operator=(const Transport&) = delete;
    Transport& operator=(Transport&&) = delete;
    virtual ~Transport() = default;

    /** Send one payload. */
    virtual bool send(const std::vector<std::uint8_t>& payload) = 0;
    /**
     * Wait up to @p timeout_ms for one payload.
     * @return false on timeout or end of stream.
     */
    virtual bool receive(std::vector<std::uint8_t>& payload, int timeout_ms) = 0;
};

}  // namespace sn::tools

#endif  // SN_TOOLS_TRANSPORT_HPP
