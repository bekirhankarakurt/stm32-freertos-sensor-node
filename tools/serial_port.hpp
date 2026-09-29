/**
 * @file serial_port.hpp
 * @brief POSIX byte stream (tty, FIFO, file or stdin) with CRC/COBS framing.
 */
#ifndef SN_TOOLS_SERIAL_PORT_HPP
#define SN_TOOLS_SERIAL_PORT_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "sn/frame.h"
#include "transport.hpp"

namespace sn::tools {

/** Framed transport over file descriptors. */
class SerialPort final : public Transport {
public:
    /**
     * @param rx_path Path to read from ("-" = stdin). TTYs are put in raw mode.
     * @param tx_path Path to write to (empty = same descriptor as @p rx_path when it is a tty).
     * @param baud    Baud rate for TTYs.
     */
    SerialPort(const std::string& rx_path, const std::string& tx_path, unsigned baud);
    SerialPort(const SerialPort&) = delete;
    SerialPort(SerialPort&&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort& operator=(SerialPort&&) = delete;
    ~SerialPort() override;

    /** True when the port opened successfully. */
    bool ok() const { return rx_fd_ >= 0; }
    /** Last error description. */
    const std::string& error() const { return error_; }

    bool send(const std::vector<std::uint8_t>& payload) override;
    bool receive(std::vector<std::uint8_t>& payload, int timeout_ms) override;

    /** Inbound frames rejected by CRC/COBS checks. */
    unsigned bad_frames() const { return bad_frames_; }
    /** True once the input reached end of file. */
    bool eof() const { return eof_; }

private:
    bool drain_pending(std::vector<std::uint8_t>& payload);

    int rx_fd_{-1};
    int tx_fd_{-1};
    bool owns_rx_{false};
    bool owns_tx_{false};
    bool eof_{false};
    unsigned bad_frames_{0U};
    std::string error_;
    sn_frame_decoder_t decoder_{};
    std::vector<std::uint8_t> pending_;
    std::size_t pending_pos_{0U};
};

}  // namespace sn::tools

#endif  // SN_TOOLS_SERIAL_PORT_HPP
