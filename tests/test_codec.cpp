/**
 * @file test_codec.cpp
 * @brief CRC, COBS, framing and ring buffer.
 */
#include <cstring>
#include <vector>

#include "sn/cobs.h"
#include "sn/crc.h"
#include "sn/frame.h"
#include "sn/ringbuf.h"
#include "sn_test.hpp"

namespace {
const std::uint8_t kCheck[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};

std::vector<std::uint8_t> cobs(const std::vector<std::uint8_t>& in)
{
    std::vector<std::uint8_t> out(SN_COBS_MAX_ENCODED(in.size()));
    out.resize(sn_cobs_encode(in.data(), in.size(), out.data(), out.size()));
    return out;
}

std::uint32_t lcg(std::uint32_t& s)
{
    s = s * 1664525U + 1013904223U;
    return s >> 8;
}
}  // namespace

TEST(crc16_check_value) { CHECK_EQ(sn_crc16(kCheck, sizeof(kCheck)), 0x29B1); }

TEST(crc32_check_value) { CHECK_EQ(sn_crc32(kCheck, sizeof(kCheck)), 0xCBF43926U); }

TEST(crc_incremental_matches_one_shot)
{
    std::uint16_t c16 = sn_crc16_update(SN_CRC16_INIT, kCheck, 4U);
    c16 = sn_crc16_update(c16, &kCheck[4], 5U);
    CHECK_EQ(c16, 0x29B1);
    std::uint32_t c32 = sn_crc32_update(SN_CRC32_INIT, kCheck, 3U);
    c32 = sn_crc32_update(c32, &kCheck[3], 6U);
    CHECK_EQ(sn_crc32_final(c32), 0xCBF43926U);
    CHECK_EQ(sn_crc16(nullptr, 0U), 0xFFFF);
}

TEST(cobs_reference_vectors)
{
    CHECK((cobs({0x00}) == std::vector<std::uint8_t>{0x01, 0x01}));
    CHECK((cobs({0x00, 0x00}) == std::vector<std::uint8_t>{0x01, 0x01, 0x01}));
    CHECK((cobs({0x11, 0x22, 0x00, 0x33}) == std::vector<std::uint8_t>{0x03, 0x11, 0x22, 0x02, 0x33}));
    CHECK((cobs({0x11, 0x00, 0x00, 0x00}) == std::vector<std::uint8_t>{0x02, 0x11, 0x01, 0x01, 0x01}));
    std::vector<std::uint8_t> run(254);
    for (std::size_t i = 0; i < run.size(); ++i) {
        run[i] = static_cast<std::uint8_t>(i + 1U);
    }
    const auto enc = cobs(run);
    CHECK_EQ(enc.size(), 256U);
    CHECK_EQ(enc[0], 0xFF);
}

TEST(cobs_round_trip_random)
{
    std::uint32_t seed = 7U;
    for (int iter = 0; iter < 300; ++iter) {
        std::vector<std::uint8_t> in(lcg(seed) % 600U);
        for (auto& b : in) {
            b = ((lcg(seed) % 4U) == 0U) ? 0U : static_cast<std::uint8_t>(lcg(seed));
        }
        const auto enc = cobs(in);
        REQUIRE(!enc.empty());
        for (std::uint8_t b : enc) {
            CHECK(b != 0U);
        }
        std::vector<std::uint8_t> dec(in.size() + 1U);
        const std::size_t n = sn_cobs_decode(enc.data(), enc.size(), dec.data(), dec.size());
        CHECK_EQ(n, in.size());
        CHECK(std::memcmp(dec.data(), in.data(), in.size()) == 0);
    }
}

TEST(cobs_rejects_malformed)
{
    std::uint8_t out[8];
    const std::uint8_t zero_code[] = {0x00, 0x11};
    const std::uint8_t overrun[] = {0x05, 0x11, 0x22};
    const std::uint8_t embedded_zero[] = {0x03, 0x11, 0x00};
    CHECK_EQ(sn_cobs_decode(zero_code, sizeof(zero_code), out, sizeof(out)), 0U);
    CHECK_EQ(sn_cobs_decode(overrun, sizeof(overrun), out, sizeof(out)), 0U);
    CHECK_EQ(sn_cobs_decode(embedded_zero, sizeof(embedded_zero), out, sizeof(out)), 0U);
    const std::uint8_t big[] = {0x04, 1, 2, 3};
    CHECK_EQ(sn_cobs_decode(big, sizeof(big), out, 2U), 0U);
    CHECK_EQ(sn_cobs_encode(big, sizeof(big), out, 2U), 0U);
}

TEST(frame_round_trip_through_stream)
{
    const std::uint8_t payload[] = {0x01, 0x00, 0x42, 0x00, 0x00, 0xFF};
    std::uint8_t wire[SN_FRAME_MAX_WIRE];
    const std::size_t n = sn_frame_encode(payload, sizeof(payload), wire, sizeof(wire));
    REQUIRE(n > sizeof(payload));
    CHECK_EQ(wire[n - 1U], 0U);

    sn_frame_decoder_t dec;
    sn_frame_decoder_init(&dec);
    std::uint8_t out[SN_FRAME_MAX_PAYLOAD];
    std::size_t len = 0U;
    /* Leading garbage terminated by a delimiter must be reported, then the frame decodes. */
    CHECK_EQ(sn_frame_decoder_push(&dec, 0x37, out, sizeof(out), &len), SN_FRAME_NONE);
    CHECK(sn_frame_decoder_push(&dec, 0x00, out, sizeof(out), &len) != SN_FRAME_OK);
    sn_frame_status_t st = SN_FRAME_NONE;
    for (std::size_t i = 0; i < n; ++i) {
        st = sn_frame_decoder_push(&dec, wire[i], out, sizeof(out), &len);
    }
    CHECK_EQ(st, SN_FRAME_OK);
    CHECK_EQ(len, sizeof(payload));
    CHECK(std::memcmp(out, payload, sizeof(payload)) == 0);
}

TEST(frame_detects_corruption_and_overflow)
{
    const std::uint8_t payload[] = {0x10, 0x05, 0x99};
    std::uint8_t wire[SN_FRAME_MAX_WIRE];
    const std::size_t n = sn_frame_encode(payload, sizeof(payload), wire, sizeof(wire));
    wire[2] ^= 0x40U;
    sn_frame_decoder_t dec;
    sn_frame_decoder_init(&dec);
    std::uint8_t out[SN_FRAME_MAX_PAYLOAD];
    std::size_t len = 0U;
    sn_frame_status_t st = SN_FRAME_NONE;
    for (std::size_t i = 0; i < n; ++i) {
        st = sn_frame_decoder_push(&dec, wire[i], out, sizeof(out), &len);
    }
    CHECK_EQ(st, SN_FRAME_ERR_CRC);

    for (std::size_t i = 0; i < SN_FRAME_MAX_BLOCK + 5U; ++i) {
        (void)sn_frame_decoder_push(&dec, 0x55, out, sizeof(out), &len);
    }
    CHECK_EQ(sn_frame_decoder_push(&dec, 0x00, out, sizeof(out), &len), SN_FRAME_ERR_OVERFLOW);

    const std::uint8_t short_block[] = {0x02, 0x11, 0x00};
    st = SN_FRAME_NONE;
    for (std::uint8_t b : short_block) {
        st = sn_frame_decoder_push(&dec, b, out, sizeof(out), &len);
    }
    CHECK_EQ(st, SN_FRAME_ERR_SHORT);
    std::uint8_t big[SN_FRAME_MAX_PAYLOAD + 1U] = {};
    CHECK_EQ(sn_frame_encode(big, sizeof(big), wire, sizeof(wire)), 0U);
    CHECK_EQ(sn_frame_encode(payload, sizeof(payload), wire, 3U), 0U);
}

TEST(frame_payload_larger_than_caller_buffer)
{
    const std::uint8_t payload[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::uint8_t wire[SN_FRAME_MAX_WIRE];
    const std::size_t n = sn_frame_encode(payload, sizeof(payload), wire, sizeof(wire));
    sn_frame_decoder_t dec;
    sn_frame_decoder_init(&dec);
    std::uint8_t out[4];
    std::size_t len = 0U;
    sn_frame_status_t st = SN_FRAME_NONE;
    for (std::size_t i = 0; i < n; ++i) {
        st = sn_frame_decoder_push(&dec, wire[i], out, sizeof(out), &len);
    }
    CHECK_EQ(st, SN_FRAME_ERR_OVERFLOW);
}

TEST(ringbuf_basic_and_wrap)
{
    std::uint8_t storage[8];
    sn_ringbuf_t rb;
    CHECK(!sn_ringbuf_init(&rb, storage, 6U));
    REQUIRE(sn_ringbuf_init(&rb, storage, sizeof(storage)));
    const std::uint8_t data[] = {1, 2, 3, 4, 5, 6};
    CHECK_EQ(sn_ringbuf_write(&rb, data, 6U), 6U);
    std::uint8_t out[8];
    CHECK_EQ(sn_ringbuf_read(&rb, out, 4U), 4U);
    CHECK_EQ(out[3], 4);
    CHECK_EQ(sn_ringbuf_write(&rb, data, 6U), 6U);
    CHECK_EQ(sn_ringbuf_free(&rb), 0U);
    CHECK(!sn_ringbuf_put(&rb, 9U));
    const std::uint8_t* lin = nullptr;
    const std::uint32_t chunk = sn_ringbuf_peek_linear(&rb, &lin);
    CHECK_EQ(chunk, 4U);
    CHECK_EQ(lin[0], 5);
    sn_ringbuf_consume(&rb, chunk);
    CHECK_EQ(sn_ringbuf_used(&rb), 4U);
    CHECK_EQ(sn_ringbuf_read(&rb, out, sizeof(out)), 4U);
    CHECK_EQ(out[0], 3);
    CHECK_EQ(out[3], 6);
    CHECK_EQ(sn_ringbuf_peek_linear(&rb, &lin), 0U);
}

TEST(ringbuf_index_wraparound)
{
    std::uint8_t storage[4];
    sn_ringbuf_t rb;
    REQUIRE(sn_ringbuf_init(&rb, storage, sizeof(storage)));
    rb.head = 0xFFFFFFFEU;
    rb.tail = 0xFFFFFFFEU;
    const std::uint8_t data[] = {7, 8, 9};
    CHECK_EQ(sn_ringbuf_write(&rb, data, 3U), 3U);
    CHECK_EQ(sn_ringbuf_used(&rb), 3U);
    std::uint8_t out[3];
    CHECK_EQ(sn_ringbuf_read(&rb, out, 3U), 3U);
    CHECK_EQ(out[2], 9);
}

SN_TEST_MAIN()
