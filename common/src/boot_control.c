/**
 * @file boot_control.c
 * @brief Boot journal persistence and A/B decision logic.
 */
#include "sn/boot_control.h"

#include "sn/crc.h"
#include "sn/le.h"

static bool slot_is_valid_index(uint8_t slot)
{
    return (slot == SN_SLOT_A) || (slot == SN_SLOT_B);
}

static uint8_t other_slot(uint8_t slot)
{
    return (slot == SN_SLOT_A) ? SN_SLOT_B : SN_SLOT_A;
}

static void encode_record(const sn_boot_state_t *s, uint32_t seq, uint8_t rec[SN_JOURNAL_RECORD_SIZE])
{
    sn_le_put32(&rec[0], SN_JOURNAL_MAGIC);
    rec[4] = s->active;
    rec[5] = s->pending;
    rec[6] = s->attempts;
    rec[7] = s->flags;
    sn_le_put32(&rec[8], seq);
    sn_le_put32(&rec[12], sn_crc32(rec, 12U));
}

static bool record_is_erased(const uint8_t rec[SN_JOURNAL_RECORD_SIZE])
{
    for (uint32_t i = 0U; i < SN_JOURNAL_RECORD_SIZE; ++i) {
        if (rec[i] != 0xFFU) {
            return false;
        }
    }
    return true;
}

static bool decode_record(const uint8_t rec[SN_JOURNAL_RECORD_SIZE], sn_boot_state_t *s, uint32_t *seq)
{
    if ((sn_le_get32(&rec[0]) != SN_JOURNAL_MAGIC) || (sn_crc32(rec, 12U) != sn_le_get32(&rec[12]))) {
        return false;
    }
    const bool active_ok = slot_is_valid_index(rec[4]) || (rec[4] == SN_SLOT_NONE);
    const bool pending_ok = slot_is_valid_index(rec[5]) || (rec[5] == SN_SLOT_NONE);
    if (!active_ok || !pending_ok) {
        return false;
    }
    s->active = rec[4];
    s->pending = rec[5];
    s->attempts = rec[6];
    s->flags = rec[7];
    *seq = sn_le_get32(&rec[8]);
    return true;
}

/* Scan the journal; returns the offset of the first erased record (== size when full). */
static uint32_t scan(const sn_journal_t *j, sn_boot_state_t *state, uint32_t *seq, bool *found)
{
    uint8_t rec[SN_JOURNAL_RECORD_SIZE];
    uint32_t offset = 0U;
    *found = false;
    *state = sn_boot_state_default();
    *seq = 0U;
    while (offset < j->size) {
        if (!j->flash->read(j->flash->ctx, j->base + offset, rec, sizeof(rec))) {
            break;
        }
        if (record_is_erased(rec)) {
            break;
        }
        sn_boot_state_t candidate;
        uint32_t candidate_seq = 0U;
        if (decode_record(rec, &candidate, &candidate_seq)) {
            *state = candidate;
            *seq = candidate_seq;
            *found = true;
        }
        offset += SN_JOURNAL_RECORD_SIZE;
    }
    return offset;
}

sn_boot_state_t sn_boot_state_default(void)
{
    const sn_boot_state_t s = {SN_SLOT_NONE, SN_SLOT_NONE, 0U, 0U};
    return s;
}

bool sn_journal_load(const sn_journal_t *j, sn_boot_state_t *state, uint32_t *seq)
{
    bool found = false;
    (void)scan(j, state, seq, &found);
    return found;
}

bool sn_journal_store(const sn_journal_t *j, const sn_boot_state_t *state)
{
    sn_boot_state_t current;
    uint32_t seq = 0U;
    bool found = false;
    uint32_t offset = scan(j, &current, &seq, &found);

    if ((offset + SN_JOURNAL_RECORD_SIZE) > j->size) {
        if (!j->flash->erase(j->flash->ctx, j->base, j->size)) {
            return false;
        }
        offset = 0U;
    }

    uint8_t rec[SN_JOURNAL_RECORD_SIZE];
    encode_record(state, seq + 1U, rec);
    if (!j->flash->program(j->flash->ctx, j->base + offset, rec, sizeof(rec))) {
        return false;
    }
    uint8_t check[SN_JOURNAL_RECORD_SIZE];
    if (!j->flash->read(j->flash->ctx, j->base + offset, check, sizeof(check))) {
        return false;
    }
    for (uint32_t i = 0U; i < SN_JOURNAL_RECORD_SIZE; ++i) {
        if (check[i] != rec[i]) {
            return false;
        }
    }
    return true;
}

static bool state_equal(const sn_boot_state_t *a, const sn_boot_state_t *b)
{
    return (a->active == b->active) && (a->pending == b->pending) && (a->attempts == b->attempts) &&
           (a->flags == b->flags);
}

sn_boot_decision_t sn_boot_decide(const sn_boot_state_t *state, bool valid_a, bool valid_b)
{
    sn_boot_decision_t d;
    d.state = *state;
    d.boot_slot = SN_SLOT_NONE;
    const bool valid[2] = {valid_a, valid_b};

    const bool has_pending = slot_is_valid_index(d.state.pending);
    if (has_pending && valid[d.state.pending] && (d.state.attempts < SN_BOOT_MAX_ATTEMPTS)) {
        /* Another trial boot of the freshly installed image. */
        d.state.attempts = (uint8_t)(d.state.attempts + 1U);
        d.state.flags = (uint8_t)(d.state.flags & (uint8_t)~SN_BOOT_FLAG_ROLLED_BACK);
        d.boot_slot = d.state.pending;
    } else {
        /* No (usable) trial: an invalid or exhausted pending image is a rollback. */
        if (has_pending) {
            d.state.flags |= SN_BOOT_FLAG_ROLLED_BACK;
        }
        d.state.pending = SN_SLOT_NONE;
        d.state.attempts = 0U;
    }

    if (d.boot_slot == SN_SLOT_NONE) {
        if (slot_is_valid_index(d.state.active) && valid[d.state.active]) {
            d.boot_slot = d.state.active;
        } else {
            const uint8_t first = slot_is_valid_index(d.state.active) ? other_slot(d.state.active) : SN_SLOT_A;
            if (valid[first]) {
                d.state.active = first;
                d.boot_slot = first;
            } else if (valid[other_slot(first)]) {
                d.state.active = other_slot(first);
                d.boot_slot = d.state.active;
            } else {
                d.state.active = SN_SLOT_NONE;
            }
        }
    }

    d.persist = !state_equal(&d.state, state);
    return d;
}

sn_boot_state_t sn_boot_mark_pending(const sn_boot_state_t *state, uint8_t slot)
{
    sn_boot_state_t s = *state;
    s.pending = slot;
    s.attempts = 0U;
    s.flags = (uint8_t)(s.flags & (uint8_t)~SN_BOOT_FLAG_ROLLED_BACK);
    return s;
}

sn_boot_state_t sn_boot_confirm(const sn_boot_state_t *state, uint8_t running, bool *changed)
{
    sn_boot_state_t s = *state;
    if ((s.pending == running) && slot_is_valid_index(running)) {
        s.active = running;
        s.pending = SN_SLOT_NONE;
        s.attempts = 0U;
    } else if (!slot_is_valid_index(s.active) && slot_is_valid_index(running)) {
        s.active = running;
    } else {
        /* Running the confirmed slot already: nothing to record. */
    }
    *changed = !state_equal(&s, state);
    return s;
}

uint8_t sn_boot_update_target(const sn_boot_state_t *state)
{
    if (slot_is_valid_index(state->active)) {
        return other_slot(state->active);
    }
    return SN_SLOT_A;
}
