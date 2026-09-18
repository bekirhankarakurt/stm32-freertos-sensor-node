/**
 * @file test_update.cpp
 * @brief Update protocol: bootloader server driven by the host uploader over an in-memory link.
 */
#include <deque>
#include <memory>

#include "image_fixture.hpp"
#include "sn/boot_control.h"
#include "sn/flash_sim.h"
#include "sn/frame.h"
#include "sn/proto_ids.h"
#include "sn/update_server.h"
#include "sn_test.hpp"
#include "uploader.hpp"

namespace {

/** Loops uploader requests straight into the server, optionally dropping replies. */
class LoopbackLink final : public sn::tools::Transport {
public:
    explicit LoopbackLink(sn_update_server_t& srv) : srv_(srv) {}

    bool send(const std::vector<std::uint8_t>& payload) override
    {
        /* Exercise the real framing in both directions. */
        std::uint8_t wire[SN_FRAME_MAX_WIRE];
        const std::size_t n = sn_frame_encode(payload.data(), payload.size(), wire, sizeof(wire));
        sn_frame_decoder_t dec;
        sn_frame_decoder_init(&dec);
        std::uint8_t req[SN_FRAME_MAX_PAYLOAD];
        std::size_t len = 0U;
        for (std::size_t i = 0; i < n; ++i) {
            if (sn_frame_decoder_push(&dec, wire[i], req, sizeof(req), &len) == SN_FRAME_OK) {
                std::uint8_t resp[64];
                sn_update_action_t action = SN_UPDATE_ACTION_NONE;
                const std::size_t r = sn_update_server_handle(&srv_, req, len, resp, sizeof(resp), &action);
                booted_ = booted_ || (action == SN_UPDATE_ACTION_BOOT);
                ++requests_;
                if ((drop_every_ != 0U) && ((requests_ % drop_every_) == 0U)) {
                    continue;
                }
                if (r > 0U) {
                    replies_.emplace_back(resp, resp + r);
                }
            }
        }
        return true;
    }

    bool receive(std::vector<std::uint8_t>& payload, int timeout_ms) override
    {
        (void)timeout_ms;
        if (replies_.empty()) {
            return false;
        }
        payload = replies_.front();
        replies_.pop_front();
        return true;
    }

    unsigned drop_every_{0U};
    bool booted_{false};
    unsigned requests_{0U};

private:
    sn_update_server_t& srv_;
    std::deque<std::vector<std::uint8_t>> replies_;
};

struct Bench {
    std::unique_ptr<sn_flash_sim_t> sim{new sn_flash_sim_t};
    sn_flash_if_t flash{};
    sn_update_server_t srv{};
    Bench()
    {
        sn_flash_sim_init(sim.get());
        flash = sn_flash_sim_if(sim.get());
        const sn_update_config_t cfg{&flash, {&flash, SN_JOURNAL_ADDR, SN_JOURNAL_SIZE},
                                     {SN_SLOT_A_ADDR, SN_SLOT_B_ADDR}, SN_SLOT_SIZE, 1U, 0U};
        sn_update_server_init(&srv, &cfg);
    }
    void install_a()
    {
        const auto img = make_image(SN_SLOT_A_ADDR, 2048U, 1U, 0U, 0U);
        CHECK(flash.program(flash.ctx, SN_SLOT_A_ADDR, img.data(), img.size()));
        const sn_boot_state_t s{SN_SLOT_A, SN_SLOT_NONE, 0U, 0U};
        const sn_journal_t j{&flash, SN_JOURNAL_ADDR, SN_JOURNAL_SIZE};
        CHECK(sn_journal_store(&j, &s));
    }
    sn_boot_state_t journal_state()
    {
        const sn_journal_t j{&flash, SN_JOURNAL_ADDR, SN_JOURNAL_SIZE};
        sn_boot_state_t s{};
        std::uint32_t seq = 0U;
        (void)sn_journal_load(&j, &s, &seq);
        return s;
    }
    std::vector<std::uint8_t> call(const std::vector<std::uint8_t>& req)
    {
        std::uint8_t resp[64];
        sn_update_action_t action = SN_UPDATE_ACTION_NONE;
        const std::size_t n = sn_update_server_handle(&srv, req.data(), req.size(), resp, sizeof(resp), &action);
        return std::vector<std::uint8_t>(resp, resp + n);
    }
};

}  // namespace

TEST(end_to_end_update_marks_inactive_slot_pending)
{
    Bench b;
    b.install_a();
    LoopbackLink link(b.srv);
    sn::tools::Uploader up(link, 10, 3);
    sn::tools::BootloaderInfo info;
    REQUIRE(up.hello(info, 0));
    CHECK_EQ(info.active, SN_SLOT_A);
    CHECK_EQ(info.target, SN_SLOT_B);
    CHECK_EQ(info.valid_mask, 0x1U);
    CHECK_EQ(info.version[0], 0x01000000U);
    CHECK_EQ(info.max_chunk, SN_UPD_MAX_CHUNK);

    const auto img = make_image(SN_SLOT_B_ADDR, 10000U, 1U, 1U, 0U);
    std::size_t last = 0U;
    REQUIRE(up.upload(img, SN_SLOT_B_ADDR, [&last](std::size_t done, std::size_t) { last = done; }));
    CHECK_EQ(last, img.size());
    const sn_boot_state_t s = b.journal_state();
    CHECK_EQ(s.active, SN_SLOT_A);
    CHECK_EQ(s.pending, SN_SLOT_B);
    CHECK_EQ(s.attempts, 0U);
    REQUIRE(up.boot());
    CHECK(link.booted_);

    /* The next reset trial-boots slot B. */
    const sn_boot_decision_t d = sn_boot_decide(&s, true, true);
    CHECK_EQ(d.boot_slot, SN_SLOT_B);
}

TEST(retries_survive_lost_replies)
{
    Bench b;
    b.install_a();
    LoopbackLink link(b.srv);
    link.drop_every_ = 4U;
    sn::tools::Uploader up(link, 10, 4);
    const auto img = make_image(SN_SLOT_B_ADDR, 3000U, 2U, 0U, 0U);
    REQUIRE(up.upload(img, SN_SLOT_B_ADDR, nullptr));
    CHECK_EQ(b.journal_state().pending, SN_SLOT_B);
}

TEST(uploader_refuses_wrong_slot_and_unsigned_images)
{
    Bench b;
    LoopbackLink link(b.srv);
    sn::tools::Uploader up(link, 10, 2);
    const auto img = make_image(SN_SLOT_A_ADDR, 1000U, 1U, 0U, 0U);
    CHECK(!up.upload(img, SN_SLOT_B_ADDR, nullptr));
    std::vector<std::uint8_t> raw(2000U, 0xAAU);
    CHECK(!up.upload(raw, SN_SLOT_A_ADDR, nullptr));
    CHECK(!up.error().empty());
    CHECK_EQ(link.requests_, 0U);
}

TEST(server_rejects_out_of_order_and_bad_requests)
{
    Bench b;
    auto r = b.call({SN_MSG_UPD_DATA, 1U, 0, 0, 0, 0, 0xAB});
    CHECK_EQ(r[3], SN_STATUS_BAD_STATE);
    r = b.call({SN_MSG_UPD_END, 2U});
    CHECK_EQ(r[3], SN_STATUS_BAD_STATE);
    r = b.call({SN_MSG_UPD_BEGIN, 3U, 0x10, 0x00});
    CHECK_EQ(r[3], SN_STATUS_BAD_LENGTH);
    r = b.call({SN_MSG_UPD_BEGIN, 4U, 0x00, 0x00, 0x10, 0x00}); /* 1 MiB > slot */
    CHECK_EQ(r[3], SN_STATUS_BAD_OFFSET);
    r = b.call({0x7F, 5U});
    CHECK_EQ(r[3], SN_STATUS_UNKNOWN);
    CHECK_EQ(r[1], 5U);
    CHECK(b.call({SN_MSG_UPD_HELLO}).empty());

    r = b.call({SN_MSG_UPD_BEGIN, 6U, 0x00, 0x04, 0x00, 0x00}); /* 1024 bytes into slot A */
    CHECK_EQ(r[3], SN_STATUS_OK);
    r = b.call({SN_MSG_UPD_DATA, 7U, 0xFF, 0x03, 0x00, 0x00, 1, 2}); /* crosses the end */
    CHECK_EQ(r[3], SN_STATUS_BAD_OFFSET);
    r = b.call({SN_MSG_UPD_DATA, 8U, 0x00, 0x00, 0x00, 0x00});
    CHECK_EQ(r[3], SN_STATUS_BAD_LENGTH);
}

TEST(server_data_is_idempotent_but_detects_conflicts)
{
    Bench b;
    auto r = b.call({SN_MSG_UPD_BEGIN, 1U, 0x00, 0x04, 0x00, 0x00});
    REQUIRE(r[3] == SN_STATUS_OK);
    const std::vector<std::uint8_t> chunk = {SN_MSG_UPD_DATA, 2U, 0x10, 0, 0, 0, 0x12, 0x34};
    CHECK_EQ(b.call(chunk)[3], SN_STATUS_OK);
    CHECK_EQ(b.call(chunk)[3], SN_STATUS_OK);
    CHECK_EQ(b.call({SN_MSG_UPD_DATA, 3U, 0x10, 0, 0, 0, 0xFF, 0xFF})[3], SN_STATUS_FLASH_ERROR);
    /* END on garbage content fails verification and leaves the journal untouched. */
    r = b.call({SN_MSG_UPD_END, 4U});
    CHECK_EQ(r[3], SN_STATUS_VERIFY_FAILED);
    CHECK_EQ(b.journal_state().pending, SN_SLOT_NONE);
}

TEST(server_rejects_size_mismatch_at_end)
{
    Bench b;
    const auto img = make_image(SN_SLOT_A_ADDR, 1000U, 1U, 0U, 0U);
    std::vector<std::uint8_t> begin = {SN_MSG_UPD_BEGIN, 1U, 0, 0, 0, 0};
    sn_le_put32(&begin[2], static_cast<std::uint32_t>(img.size() + 4U));
    REQUIRE(b.call(begin)[3] == SN_STATUS_OK);
    CHECK(b.flash.program(b.flash.ctx, SN_SLOT_A_ADDR, img.data(), img.size()));
    const auto r = b.call({SN_MSG_UPD_END, 2U});
    CHECK_EQ(r[3], SN_STATUS_VERIFY_FAILED);
    CHECK_EQ(sn_le_get32(&r[4]), SN_IMAGE_ERR_SIZE);
}

TEST(server_reports_flash_errors)
{
    Bench b;
    REQUIRE(b.call({SN_MSG_UPD_BEGIN, 1U, 0x00, 0x04, 0x00, 0x00})[3] == SN_STATUS_OK);
    b.sim->fail_after = 1U;
    CHECK_EQ(b.call({SN_MSG_UPD_DATA, 2U, 0, 0, 0, 0, 0x11})[3], SN_STATUS_FLASH_ERROR);

    Bench c;
    const auto img = make_image(SN_SLOT_A_ADDR, 1000U, 1U, 0U, 0U);
    std::vector<std::uint8_t> begin = {SN_MSG_UPD_BEGIN, 1U, 0, 0, 0, 0};
    sn_le_put32(&begin[2], static_cast<std::uint32_t>(img.size()));
    REQUIRE(c.call(begin)[3] == SN_STATUS_OK);
    CHECK(c.flash.program(c.flash.ctx, SN_SLOT_A_ADDR, img.data(), img.size()));
    c.sim->fail_after = 1U; /* journal write fails */
    CHECK_EQ(c.call({SN_MSG_UPD_END, 2U})[3], SN_STATUS_FLASH_ERROR);
}

SN_TEST_MAIN()
