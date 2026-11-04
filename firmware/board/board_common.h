/**
 * @file board_common.h
 * @brief Initialisation shared by the target boards.
 */
#ifndef SN_BOARD_COMMON_H
#define SN_BOARD_COMMON_H

/** @brief Latch the reset cause and configure the status LED (PA5, LD2). */
void board_common_init(void);

#endif /* SN_BOARD_COMMON_H */
