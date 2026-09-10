/**
 * @file update_server.c
 * @brief Firmware update protocol state machine.
 */
#include "sn/update_server.h"

#include <string.h>

#include "sn/image.h"
#include "sn/le.h"
#include "sn/proto_ids.h"

#define RESULT_LEN 8U
#define INFO_LEN   SN_UPDATE_MAX_RESPONSE

void sn_update_server_init(sn_update_server_t *srv, const sn_update_config_t *cfg)
{
    srv->cfg = *cfg;
    srv->session = false;
    srv->target = SN_SLOT_NONE;
    srv->total = 0U;
    srv->has_last = false;
    srv->last_type = 0U;
    srv->last_seq = 0U;
    srv->last_len = 0U;
}

static size_t result(uint8_t *resp, uint8_t seq, uint8_t cmd, uint8_t status, uint32_t arg)
{
    resp[0] = SN_MSG_UPD_RESULT;
    resp[1] = seq;
    resp[2] = cmd;
    resp[3] = status;
    sn_le_put32(&resp[4], arg);
    return RESULT_LEN;
}

static size_t handle_hello(sn_update_server_t *srv, uint8_t seq, uint8_t *resp)
{
    sn_boot_state_t state;
    uint32_t jseq = 0U;
    (void)sn_journal_load(&srv->cfg.journal, &state, &jseq);

    uint8_t valid_mask = 0U;
    uint32_t versions[2] = {0U, 0U};
    for (uint8_t s = 0U; s < 2U; ++s) {
        sn_image_header_t hdr;
        if (sn_image_validate(srv->cfg.flash, srv->cfg.slot_addr[s], srv->cfg.slot_size, &hdr) == SN_IMAGE_OK) {
            valid_mask = (uint8_t)(valid_mask | (uint8_t)(1U << s));
            versions[s] = sn_image_version_ordinal(&hdr);
        }
    }
    resp[0] = SN_MSG_UPD_INFO;
    resp[1] = seq;
    resp[2] = SN_UPD_PROTOCOL_VERSION;
    resp[3] = srv->cfg.bl_major;
    resp[4] = srv->cfg.bl_minor;
    resp[5] = state.active;
    resp[6] = state.pending;
    resp[7] = sn_boot_update_target(&state);
    resp[8] = valid_mask;
    resp[9] = state.flags;
    sn_le_put16(&resp[10], (uint16_t)SN_UPD_MAX_CHUNK);
    sn_le_put32(&resp[12], versions[0]);
    sn_le_put32(&resp[16], versions[1]);
    return INFO_LEN;
}

static size_t handle_begin(sn_update_server_t *srv, const uint8_t *req, size_t len, uint8_t *resp)
{
    const uint8_t seq = req[1];
    if (len != 6U) {
        return result(resp, seq, SN_MSG_UPD_BEGIN, SN_STATUS_BAD_LENGTH, 0U);
    }
    const uint32_t total = sn_le_get32(&req[2]);
    if ((total <= SN_IMAGE_HEADER_AREA) || (total > srv->cfg.slot_size)) {
        return result(resp, seq, SN_MSG_UPD_BEGIN, SN_STATUS_BAD_OFFSET, total);
    }
    sn_boot_state_t state;
    uint32_t jseq = 0U;
    (void)sn_journal_load(&srv->cfg.journal, &state, &jseq);
    const uint8_t target = sn_boot_update_target(&state);

    srv->session = false;
    if (!srv->cfg.flash->erase(srv->cfg.flash->ctx, srv->cfg.slot_addr[target], srv->cfg.slot_size)) {
        return result(resp, seq, SN_MSG_UPD_BEGIN, SN_STATUS_FLASH_ERROR, 0U);
    }
    srv->session = true;
    srv->target = target;
    srv->total = total;
    return result(resp, seq, SN_MSG_UPD_BEGIN, SN_STATUS_OK, srv->cfg.slot_addr[target]);
}

static bool flash_matches(const sn_flash_if_t *flash, uint32_t addr, const uint8_t *data, size_t len)
{
    uint8_t buf[32];
    size_t done = 0U;
    while (done < len) {
        const size_t n = ((len - done) < sizeof(buf)) ? (len - done) : sizeof(buf);
        if (!flash->read(flash->ctx, addr + (uint32_t)done, buf, n)) {
            return false;
        }
        for (size_t i = 0U; i < n; ++i) {
            if (buf[i] != data[done + i]) {
                return false;
            }
        }
        done += n;
    }
    return true;
}

static size_t handle_data(sn_update_server_t *srv, const uint8_t *req, size_t len, uint8_t *resp)
{
    const uint8_t seq = req[1];
    if ((len < 7U) || ((len - 6U) > SN_UPD_MAX_CHUNK)) {
        return result(resp, seq, SN_MSG_UPD_DATA, SN_STATUS_BAD_LENGTH, 0U);
    }
    if (!srv->session) {
        return result(resp, seq, SN_MSG_UPD_DATA, SN_STATUS_BAD_STATE, 0U);
    }
    const uint32_t offset = sn_le_get32(&req[2]);
    const uint32_t n = (uint32_t)(len - 6U);
    if ((offset > srv->total) || (n > (srv->total - offset))) {
        return result(resp, seq, SN_MSG_UPD_DATA, SN_STATUS_BAD_OFFSET, offset);
    }
    const uint32_t addr = srv->cfg.slot_addr[srv->target] + offset;
    const uint8_t *data = &req[6];
    if (!flash_matches(srv->cfg.flash, addr, data, n)) {
        if (!srv->cfg.flash->program(srv->cfg.flash->ctx, addr, data, n) ||
            !flash_matches(srv->cfg.flash, addr, data, n)) {
            return result(resp, seq, SN_MSG_UPD_DATA, SN_STATUS_FLASH_ERROR, offset);
        }
    }
    return result(resp, seq, SN_MSG_UPD_DATA, SN_STATUS_OK, offset + n);
}

static size_t handle_end(sn_update_server_t *srv, uint8_t seq, uint8_t *resp)
{
    if (!srv->session) {
        return result(resp, seq, SN_MSG_UPD_END, SN_STATUS_BAD_STATE, 0U);
    }
    sn_image_header_t hdr;
    const sn_image_status_t st =
        sn_image_validate(srv->cfg.flash, srv->cfg.slot_addr[srv->target], srv->cfg.slot_size, &hdr);
    if (st != SN_IMAGE_OK) {
        return result(resp, seq, SN_MSG_UPD_END, SN_STATUS_VERIFY_FAILED, (uint32_t)st);
    }
    if ((hdr.image_size + SN_IMAGE_HEADER_AREA) != srv->total) {
        return result(resp, seq, SN_MSG_UPD_END, SN_STATUS_VERIFY_FAILED, (uint32_t)SN_IMAGE_ERR_SIZE);
    }
    sn_boot_state_t state;
    uint32_t jseq = 0U;
    (void)sn_journal_load(&srv->cfg.journal, &state, &jseq);
    const sn_boot_state_t next = sn_boot_mark_pending(&state, srv->target);
    if (!sn_journal_store(&srv->cfg.journal, &next)) {
        return result(resp, seq, SN_MSG_UPD_END, SN_STATUS_FLASH_ERROR, 0U);
    }
    srv->session = false;
    return result(resp, seq, SN_MSG_UPD_END, SN_STATUS_OK, hdr.image_crc32);
}

size_t sn_update_server_handle(sn_update_server_t *srv, const uint8_t *req, size_t len, uint8_t *resp,
                               size_t cap, sn_update_action_t *action)
{
    *action = SN_UPDATE_ACTION_NONE;
    if ((len < 2U) || (cap < SN_UPDATE_MAX_RESPONSE)) {
        return 0U;
    }
    const uint8_t type = req[0];
    const uint8_t seq = req[1];
    if (srv->has_last && (type == srv->last_type) && (seq == srv->last_seq) && (type != SN_MSG_UPD_HELLO)) {
        (void)memcpy(resp, srv->last_resp, srv->last_len);
        *action = (type == SN_MSG_UPD_BOOT) ? SN_UPDATE_ACTION_BOOT : SN_UPDATE_ACTION_NONE;
        return srv->last_len;
    }
    size_t out = 0U;
    switch (type) {
    case SN_MSG_UPD_HELLO:
        out = handle_hello(srv, seq, resp);
        break;
    case SN_MSG_UPD_BEGIN:
        out = handle_begin(srv, req, len, resp);
        break;
    case SN_MSG_UPD_DATA:
        out = handle_data(srv, req, len, resp);
        break;
    case SN_MSG_UPD_END:
        out = handle_end(srv, seq, resp);
        break;
    case SN_MSG_UPD_BOOT:
        *action = SN_UPDATE_ACTION_BOOT;
        out = result(resp, seq, SN_MSG_UPD_BOOT, SN_STATUS_OK, 0U);
        break;
    default:
        out = result(resp, seq, type, SN_STATUS_UNKNOWN, 0U);
        break;
    }
    srv->has_last = true;
    srv->last_type = type;
    srv->last_seq = seq;
    srv->last_len = out;
    (void)memcpy(srv->last_resp, resp, out);
    return out;
}
