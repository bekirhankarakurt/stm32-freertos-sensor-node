/**
 * @file ringbuf.c
 * @brief SPSC ring buffer using GCC/Clang atomic builtins for index publication.
 */
#include "sn/ringbuf.h"

static uint32_t load_acquire(const uint32_t *p)
{
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

static void store_release(uint32_t *p, uint32_t v)
{
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

bool sn_ringbuf_init(sn_ringbuf_t *rb, uint8_t *storage, uint32_t capacity)
{
    if ((storage == NULL) || (capacity < 2U) || ((capacity & (capacity - 1U)) != 0U)) {
        return false;
    }
    rb->storage = storage;
    rb->mask = capacity - 1U;
    rb->head = 0U;
    rb->tail = 0U;
    return true;
}

uint32_t sn_ringbuf_used(const sn_ringbuf_t *rb)
{
    return load_acquire(&rb->head) - load_acquire(&rb->tail);
}

uint32_t sn_ringbuf_free(const sn_ringbuf_t *rb)
{
    return (rb->mask + 1U) - sn_ringbuf_used(rb);
}

uint32_t sn_ringbuf_write(sn_ringbuf_t *rb, const uint8_t *data, uint32_t len)
{
    const uint32_t head = rb->head;
    const uint32_t space = (rb->mask + 1U) - (head - load_acquire(&rb->tail));
    const uint32_t n = (len < space) ? len : space;
    for (uint32_t i = 0U; i < n; ++i) {
        rb->storage[(head + i) & rb->mask] = data[i];
    }
    store_release(&rb->head, head + n);
    return n;
}

bool sn_ringbuf_put(sn_ringbuf_t *rb, uint8_t byte)
{
    return sn_ringbuf_write(rb, &byte, 1U) == 1U;
}

uint32_t sn_ringbuf_read(sn_ringbuf_t *rb, uint8_t *dst, uint32_t cap)
{
    const uint32_t tail = rb->tail;
    const uint32_t avail = load_acquire(&rb->head) - tail;
    const uint32_t n = (cap < avail) ? cap : avail;
    for (uint32_t i = 0U; i < n; ++i) {
        dst[i] = rb->storage[(tail + i) & rb->mask];
    }
    store_release(&rb->tail, tail + n);
    return n;
}

uint32_t sn_ringbuf_peek_linear(const sn_ringbuf_t *rb, const uint8_t **data)
{
    const uint32_t tail = rb->tail;
    const uint32_t avail = load_acquire(&rb->head) - tail;
    const uint32_t offset = tail & rb->mask;
    const uint32_t to_end = (rb->mask + 1U) - offset;
    *data = &rb->storage[offset];
    return (avail < to_end) ? avail : to_end;
}

void sn_ringbuf_consume(sn_ringbuf_t *rb, uint32_t len)
{
    store_release(&rb->tail, rb->tail + len);
}
