/**
 * @file test_boot.cpp
 * @brief Image validation, boot journal and A/B decision logic.
 */
#include <memory>

#include "image_fixture.hpp"
#include "sn/boot_control.h"
#include "sn/flash_sim.h"
#include "sn_test.hpp"

namespace {

struct Fixture {
    std::unique_ptr<sn_flash_sim_t> sim{new sn_flash_sim_t};
    sn_flash_if_t flash{};
    sn_journal_t journal{};
    Fixture()
    {
        sn_flash_sim_init(sim.get());
        flash = sn_flash_sim_if(sim.get());
        journal = sn_journal_t{&flash, SN_JOURNAL_ADDR, SN_JOURNAL_SIZE};
    }
    void write(std::uint32_t addr, const std::vector<std::uint8_t>& data)
    {
        REQUIRE_OK(flash.program(flash.ctx, addr, data.data(), data.size()));
    }
    static void REQUIRE_OK(bool ok) { CHECK(ok); }
    sn_image_status_t validate(std::uint32_t slot)
    {
        sn_image_header_t hdr{};
        return sn_image_validate(&flash, slot, SN_SLOT_SIZE, &hdr);
    }
};

sn_boot_state_t state(std::uint8_t active, std::uint8_t pending, std::uint8_t attempts, std::uint8_t flags)
{
    return sn_boot_state_t{active, pending, attempts, flags};
}

}  // namespace

TEST(image_valid_and_header_round_trip)
{
    Fixture f;
    const auto img = make_image(SN_SLOT_A_ADDR, 4096U, 1U, 2U, 3U);
    f.write(SN_SLOT_A_ADDR, img);
    sn_image_header_t hdr{};
    CHECK_EQ(sn_image_validate(&f.flash, SN_SLOT_A_ADDR, SN_SLOT_SIZE, &hdr), SN_IMAGE_OK);
    CHECK_EQ(hdr.image_size, 4096U);
    CHECK_EQ(sn_image_version_ordinal(&hdr), 0x01020003U);
    std::uint8_t raw[SN_IMAGE_HEADER_SIZE];
    sn_image_header_encode(&hdr, raw);
    CHECK_EQ(sn_image_header_check(raw), SN_IMAGE_OK);
}

TEST(image_rejections)
{
    Fixture f;
    CHECK_EQ(f.validate(SN_SLOT_A_ADDR), SN_IMAGE_ERR_MAGIC);

    auto img = make_image(SN_SLOT_A_ADDR, 1024U, 1U, 0U, 0U);
    f.write(SN_SLOT_B_ADDR, img);
    CHECK_EQ(f.validate(SN_SLOT_B_ADDR), SN_IMAGE_ERR_ADDRESS);

    img[SN_IMAGE_HEADER_AREA + 100U] ^= 0x01U;
    f.write(SN_SLOT_A_ADDR, img);
    CHECK_EQ(f.validate(SN_SLOT_A_ADDR), SN_IMAGE_ERR_CRC);

    sn_flash_sim_init(f.sim.get());
    auto hdr_bad = make_image(SN_SLOT_A_ADDR, 1024U, 1U, 0U, 0U);
    hdr_bad[9] ^= 0x10U;
    f.write(SN_SLOT_A_ADDR, hdr_bad);
    CHECK_EQ(f.validate(SN_SLOT_A_ADDR), SN_IMAGE_ERR_HEADER);

    sn_image_header_t hdr{};
    CHECK_EQ(sn_image_validate(&f.flash, 0x20000000U, SN_SLOT_SIZE, &hdr), SN_IMAGE_ERR_READ);
}

TEST(image_rejects_oversize_and_bad_vectors)
{
    Fixture f;
    auto img = make_image(SN_SLOT_A_ADDR, 1024U, 1U, 0U, 0U);
    sn_image_header_t hdr{};
    sn_image_header_decode(img.data(), &hdr);
    hdr.image_size = SN_SLOT_SIZE;
    sn_image_header_encode(&hdr, img.data());
    f.write(SN_SLOT_A_ADDR, img);
    CHECK_EQ(f.validate(SN_SLOT_A_ADDR), SN_IMAGE_ERR_SIZE);

    sn_flash_sim_init(f.sim.get());
    auto vec = make_image(SN_SLOT_A_ADDR, 1024U, 1U, 0U, 0U);
    sn_le_put32(&vec[SN_IMAGE_HEADER_AREA + 4U], 0x08000101U); /* reset vector outside the image */
    sn_image_header_decode(vec.data(), &hdr);
    hdr.image_crc32 = sn_crc32(&vec[SN_IMAGE_HEADER_AREA], hdr.image_size);
    sn_image_header_encode(&hdr, vec.data());
    f.write(SN_SLOT_A_ADDR, vec);
    CHECK_EQ(f.validate(SN_SLOT_A_ADDR), SN_IMAGE_ERR_VECTORS);
}

TEST(journal_blank_returns_default)
{
    Fixture f;
    sn_boot_state_t s{};
    std::uint32_t seq = 99U;
    CHECK(!sn_journal_load(&f.journal, &s, &seq));
    CHECK_EQ(s.active, SN_SLOT_NONE);
    CHECK_EQ(s.pending, SN_SLOT_NONE);
    CHECK_EQ(seq, 0U);
}

TEST(journal_appends_and_loads_latest)
{
    Fixture f;
    const sn_boot_state_t a = state(SN_SLOT_A, SN_SLOT_NONE, 0U, 0U);
    const sn_boot_state_t b = state(SN_SLOT_A, SN_SLOT_B, 2U, 0U);
    REQUIRE(sn_journal_store(&f.journal, &a));
    REQUIRE(sn_journal_store(&f.journal, &b));
    sn_boot_state_t s{};
    std::uint32_t seq = 0U;
    REQUIRE(sn_journal_load(&f.journal, &s, &seq));
    CHECK_EQ(seq, 2U);
    CHECK_EQ(s.pending, SN_SLOT_B);
    CHECK_EQ(s.attempts, 2U);
    CHECK_EQ(f.sim->erase_count, 0U);
}

TEST(journal_compacts_when_full)
{
    Fixture f;
    const std::uint32_t capacity = SN_JOURNAL_SIZE / SN_JOURNAL_RECORD_SIZE;
    for (std::uint32_t i = 0U; i < capacity + 3U; ++i) {
        const sn_boot_state_t s = state(SN_SLOT_A, SN_SLOT_B, static_cast<std::uint8_t>(i & 0x3U), 0U);
        REQUIRE(sn_journal_store(&f.journal, &s));
    }
    CHECK_EQ(f.sim->erase_count, 1U);
    sn_boot_state_t s{};
    std::uint32_t seq = 0U;
    REQUIRE(sn_journal_load(&f.journal, &s, &seq));
    CHECK_EQ(seq, capacity + 3U);
    CHECK_EQ(s.attempts, (capacity + 2U) & 0x3U);
}

TEST(journal_skips_torn_record)
{
    Fixture f;
    const sn_boot_state_t a = state(SN_SLOT_B, SN_SLOT_NONE, 0U, 0U);
    REQUIRE(sn_journal_store(&f.journal, &a));
    /* Simulate a power cut half-way through the next record. */
    const std::vector<std::uint8_t> torn = {0x4A, 0x4E, 0x52, 0x4C, 0x00, 0x01, 0x00};
    f.write(SN_JOURNAL_ADDR + SN_JOURNAL_RECORD_SIZE, torn);
    sn_boot_state_t s{};
    std::uint32_t seq = 0U;
    REQUIRE(sn_journal_load(&f.journal, &s, &seq));
    CHECK_EQ(s.active, SN_SLOT_B);
    /* The next store goes after the torn record and supersedes it. */
    const sn_boot_state_t b = state(SN_SLOT_A, SN_SLOT_NONE, 0U, 0U);
    REQUIRE(sn_journal_store(&f.journal, &b));
    REQUIRE(sn_journal_load(&f.journal, &s, &seq));
    CHECK_EQ(s.active, SN_SLOT_A);
    CHECK_EQ(seq, 2U);
}

TEST(journal_reports_flash_failure)
{
    Fixture f;
    f.sim->fail_after = 1U;
    const sn_boot_state_t a = state(SN_SLOT_A, SN_SLOT_NONE, 0U, 0U);
    CHECK(!sn_journal_store(&f.journal, &a));
}

TEST(decide_fresh_device)
{
    const sn_boot_state_t blank = sn_boot_state_default();
    sn_boot_decision_t d = sn_boot_decide(&blank, true, false);
    CHECK_EQ(d.boot_slot, SN_SLOT_A);
    CHECK_EQ(d.state.active, SN_SLOT_A);
    CHECK(d.persist);

    d = sn_boot_decide(&blank, false, true);
    CHECK_EQ(d.boot_slot, SN_SLOT_B);

    d = sn_boot_decide(&blank, false, false);
    CHECK_EQ(d.boot_slot, SN_SLOT_NONE);
    CHECK(!d.persist);
}

TEST(decide_confirmed_slot_is_stable)
{
    const sn_boot_state_t s = state(SN_SLOT_B, SN_SLOT_NONE, 0U, 0U);
    const sn_boot_decision_t d = sn_boot_decide(&s, true, true);
    CHECK_EQ(d.boot_slot, SN_SLOT_B);
    CHECK(!d.persist);
}

TEST(decide_falls_back_when_active_corrupt)
{
    const sn_boot_state_t s = state(SN_SLOT_A, SN_SLOT_NONE, 0U, 0U);
    const sn_boot_decision_t d = sn_boot_decide(&s, false, true);
    CHECK_EQ(d.boot_slot, SN_SLOT_B);
    CHECK_EQ(d.state.active, SN_SLOT_B);
    CHECK(d.persist);
}

TEST(trial_boot_then_rollback_after_max_attempts)
{
    const sn_boot_state_t confirmed = state(SN_SLOT_A, SN_SLOT_NONE, 0U, SN_BOOT_FLAG_ROLLED_BACK);
    sn_boot_state_t s = sn_boot_mark_pending(&confirmed, SN_SLOT_B);
    CHECK_EQ(s.flags, 0U);
    for (std::uint8_t i = 1U; i <= SN_BOOT_MAX_ATTEMPTS; ++i) {
        const sn_boot_decision_t d = sn_boot_decide(&s, true, true);
        CHECK_EQ(d.boot_slot, SN_SLOT_B);
        CHECK_EQ(d.state.attempts, i);
        CHECK(d.persist);
        s = d.state;
    }
    const sn_boot_decision_t d = sn_boot_decide(&s, true, true);
    CHECK_EQ(d.boot_slot, SN_SLOT_A);
    CHECK_EQ(d.state.pending, SN_SLOT_NONE);
    CHECK((d.state.flags & SN_BOOT_FLAG_ROLLED_BACK) != 0U);
}

TEST(trial_of_invalid_image_rolls_back_immediately)
{
    const sn_boot_state_t s = state(SN_SLOT_A, SN_SLOT_B, 0U, 0U);
    const sn_boot_decision_t d = sn_boot_decide(&s, true, false);
    CHECK_EQ(d.boot_slot, SN_SLOT_A);
    CHECK_EQ(d.state.pending, SN_SLOT_NONE);
    CHECK((d.state.flags & SN_BOOT_FLAG_ROLLED_BACK) != 0U);
}

TEST(corrupt_pending_index_is_cleared)
{
    const sn_boot_state_t s = state(SN_SLOT_A, 7U, 1U, 0U);
    const sn_boot_decision_t d = sn_boot_decide(&s, true, true);
    CHECK_EQ(d.boot_slot, SN_SLOT_A);
    CHECK_EQ(d.state.pending, SN_SLOT_NONE);
}

TEST(confirm_promotes_trial_slot)
{
    const sn_boot_state_t s = state(SN_SLOT_A, SN_SLOT_B, 1U, SN_BOOT_FLAG_ROLLED_BACK);
    bool changed = false;
    sn_boot_state_t n = sn_boot_confirm(&s, SN_SLOT_B, &changed);
    CHECK(changed);
    CHECK_EQ(n.active, SN_SLOT_B);
    CHECK_EQ(n.pending, SN_SLOT_NONE);
    CHECK_EQ(n.attempts, 0U);

    n = sn_boot_confirm(&n, SN_SLOT_B, &changed);
    CHECK(!changed);

    const sn_boot_state_t blank = sn_boot_state_default();
    n = sn_boot_confirm(&blank, SN_SLOT_A, &changed);
    CHECK(changed);
    CHECK_EQ(n.active, SN_SLOT_A);
}

TEST(update_target_never_overwrites_confirmed)
{
    sn_boot_state_t s = state(SN_SLOT_A, SN_SLOT_NONE, 0U, 0U);
    CHECK_EQ(sn_boot_update_target(&s), SN_SLOT_B);
    s.active = SN_SLOT_B;
    CHECK_EQ(sn_boot_update_target(&s), SN_SLOT_A);
    s = sn_boot_state_default();
    CHECK_EQ(sn_boot_update_target(&s), SN_SLOT_A);
}

SN_TEST_MAIN()
