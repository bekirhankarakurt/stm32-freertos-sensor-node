/**
 * @file ringbuf.h
 * @brief Lock-free single-producer/single-consumer byte ring buffer.
 *
 * Safe between one interrupt handler and one task (or two threads) without a
 * critical section: the producer only writes @c head, the consumer only
 * writes @c tail, and index publication uses acquire/release ordering.
 */
#ifndef SN_RINGBUF_H
#define SN_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ring buffer control block; storage is supplied by the owner. */
typedef struct {
    uint8_t *storage; /**< Backing storage. */
    uint32_t mask;    /**< Capacity - 1 (capacity is a power of two). */
    uint32_t head;    /**< Free-running write index (producer-owned). */
    uint32_t tail;    /**< Free-running read index (consumer-owned). */
} sn_ringbuf_t;

/**
 * @brief Initialise a ring buffer.
 * @param capacity Size of @p storage; must be a power of two >= 2.
 * @return false if @p capacity is not a power of two.
 */
bool sn_ringbuf_init(sn_ringbuf_t *rb, uint8_t *storage, uint32_t capacity);

/** @brief Bytes available to read. */
uint32_t sn_ringbuf_used(const sn_ringbuf_t *rb);

/** @brief Bytes that can still be written. */
uint32_t sn_ringbuf_free(const sn_ringbuf_t *rb);

/** @brief Producer: append up to @p len bytes, returns bytes written. */
uint32_t sn_ringbuf_write(sn_ringbuf_t *rb, const uint8_t *data, uint32_t len);

/** @brief Producer: append one byte, returns false when full. */
bool sn_ringbuf_put(sn_ringbuf_t *rb, uint8_t byte);

/** @brief Consumer: copy out up to @p cap bytes, returns bytes read. */
uint32_t sn_ringbuf_read(sn_ringbuf_t *rb, uint8_t *dst, uint32_t cap);

/**
 * @brief Consumer: largest contiguous readable block (for DMA).
 * @param[out] data Start of the block.
 * @return Length of the block (0 when empty).
 */
uint32_t sn_ringbuf_peek_linear(const sn_ringbuf_t *rb, const uint8_t **data);

/** @brief Consumer: release @p len bytes previously peeked. */
void sn_ringbuf_consume(sn_ringbuf_t *rb, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* SN_RINGBUF_H */
