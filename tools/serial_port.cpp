/**
 * @file serial_port.cpp
 * @brief POSIX framed transport.
 */
#include "serial_port.hpp"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

namespace sn::tools {
namespace {

speed_t to_speed(unsigned baud)
{
    switch (baud) {
    case 9600U:
        return B9600;
    case 19200U:
        return B19200;
    case 38400U:
        return B38400;
    case 57600U:
        return B57600;
    case 230400U:
        return B230400;
    case 460800U:
        return B460800;
    case 921600U:
        return B921600;
    default:
        return B115200;
    }
}

bool make_raw(int fd, unsigned baud)
{
    termios tio{};
    if (tcgetattr(fd, &tio) != 0) {
        return false;
    }
    cfmakeraw(&tio);
    tio.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    const speed_t speed = to_speed(baud);
    (void)cfsetispeed(&tio, speed);
    (void)cfsetospeed(&tio, speed);
    return tcsetattr(fd, TCSANOW, &tio) == 0;
}

}  // namespace

SerialPort::SerialPort(const std::string& rx_path, const std::string& tx_path, unsigned baud)
{
    sn_frame_decoder_init(&decoder_);
    if (rx_path == "-") {
        rx_fd_ = STDIN_FILENO;
    } else {
        rx_fd_ = ::open(rx_path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (rx_fd_ < 0) {
            rx_fd_ = ::open(rx_path.c_str(), O_RDONLY | O_NONBLOCK);
        }
        owns_rx_ = rx_fd_ >= 0;
    }
    if (rx_fd_ < 0) {
        error_ = rx_path + ": " + std::strerror(errno);
        return;
    }
    const bool tty = isatty(rx_fd_) == 1;
    if (tty && !make_raw(rx_fd_, baud)) {
        error_ = rx_path + ": cannot configure tty";
    }
    if (!tx_path.empty()) {
        tx_fd_ = ::open(tx_path.c_str(), O_WRONLY | O_NOCTTY | O_CREAT, 0644);
        struct stat st {};
        if ((tx_fd_ >= 0) && (fstat(tx_fd_, &st) == 0) && S_ISREG(st.st_mode)) {
            (void)ftruncate(tx_fd_, 0);
        }
        owns_tx_ = tx_fd_ >= 0;
        if (tx_fd_ < 0) {
            error_ = tx_path + ": " + std::strerror(errno);
        }
    } else if (tty) {
        tx_fd_ = rx_fd_;
    }
}

SerialPort::~SerialPort()
{
    if (owns_tx_) {
        (void)::close(tx_fd_);
    }
    if (owns_rx_) {
        (void)::close(rx_fd_);
    }
}

bool SerialPort::send(const std::vector<std::uint8_t>& payload)
{
    if (tx_fd_ < 0) {
        return false;
    }
    std::vector<std::uint8_t> wire(SN_FRAME_MAX_WIRE);
    const std::size_t n = sn_frame_encode(payload.data(), payload.size(), wire.data(), wire.size());
    if (n == 0U) {
        return false;
    }
    std::size_t done = 0U;
    while (done < n) {
        const ssize_t w = ::write(tx_fd_, &wire[done], n - done);
        if (w > 0) {
            done += static_cast<std::size_t>(w);
        } else if ((w < 0) && (errno != EINTR) && (errno != EAGAIN)) {
            return false;
        }
    }
    return true;
}

bool SerialPort::drain_pending(std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> out(SN_FRAME_MAX_PAYLOAD);
    while (pending_pos_ < pending_.size()) {
        std::size_t len = 0U;
        const std::uint8_t byte = pending_[pending_pos_];
        ++pending_pos_;
        const sn_frame_status_t st = sn_frame_decoder_push(&decoder_, byte, out.data(), out.size(), &len);
        if (st == SN_FRAME_OK) {
            payload.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(len));
            return true;
        }
        if (st != SN_FRAME_NONE) {
            ++bad_frames_;
        }
    }
    pending_.clear();
    pending_pos_ = 0U;
    return false;
}

bool SerialPort::receive(std::vector<std::uint8_t>& payload, int timeout_ms)
{
    int remaining = timeout_ms;
    for (;;) {
        if (drain_pending(payload)) {
            return true;
        }
        if (eof_) {
            return false;
        }
        pollfd pfd{rx_fd_, POLLIN, 0};
        const int step = ((remaining >= 0) && (remaining < 20)) ? remaining : 20;
        const int pr = ::poll(&pfd, 1, step);
        if ((pr < 0) && (errno != EINTR)) {
            return false;
        }
        if (pr <= 0) {
            if (remaining >= 0) {
                remaining -= step;
                if (remaining <= 0) {
                    return false;
                }
            }
            continue;
        }
        std::uint8_t buf[256];
        const ssize_t r = ::read(rx_fd_, buf, sizeof(buf));
        if (r > 0) {
            pending_.assign(buf, buf + r);
            pending_pos_ = 0U;
        } else if ((r == 0) && (isatty(rx_fd_) == 0)) {
            eof_ = true;
        } else if ((r < 0) && (errno != EAGAIN) && (errno != EINTR)) {
            return false;
        } else {
            /* Spurious wake-up: poll again. */
        }
    }
}

}  // namespace sn::tools
