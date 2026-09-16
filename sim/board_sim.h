/**
 * @file board_sim.h
 * @brief Linux simulation board options.
 */
#ifndef SN_BOARD_SIM_H
#define SN_BOARD_SIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Exit code when the firmware requests a software reset. */
#define SN_SIM_EXIT_RESET      10
/** Exit code when the firmware requests the bootloader. */
#define SN_SIM_EXIT_BOOTLOADER 11
/** Exit code when the emulated watchdog expires. */
#define SN_SIM_EXIT_WATCHDOG   12

/** Simulation options. */
typedef struct {
    int out_fd;            /**< Destination of the UART TX stream. */
    int in_fd;             /**< Source of UART RX bytes (-1 = none). */
    uint32_t duration_ms;  /**< Stop after this time (0 = run forever). */
    uint32_t seed;         /**< Environment model seed. */
} sn_sim_options_t;

/** @brief Apply options; call before board_init(). */
void board_sim_configure(const sn_sim_options_t *opt);

/** @brief Idle-time housekeeping: watchdog emulation and end of run (exits the process). */
void board_sim_idle(void);

#ifdef __cplusplus
}
#endif

#endif /* SN_BOARD_SIM_H */
