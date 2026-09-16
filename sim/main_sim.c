/**
 * @file main_sim.c
 * @brief Linux simulation entry point: the complete application on the FreeRTOS POSIX port.
 *
 * Usage: sn_sim [--duration-ms N] [--seed N] [--input PATH] [--output PATH]
 * The binary telemetry stream goes to stdout (or --output); pipe it into
 * sn_decode. Commands are read from --input (e.g. a FIFO written by sn_decode).
 */
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "board_sim.h"
#include "node.h"

static void usage(const char *prog)
{
    (void)fprintf(stderr, "usage: %s [--duration-ms N] [--seed N] [--input PATH] [--output PATH]\n", prog);
}

static int parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    const unsigned long v = strtoul(text, &end, 0);
    if ((end == text) || (*end != '\0') || (v > 0xFFFFFFFFUL)) {
        return -1;
    }
    *out = (uint32_t)v;
    return 0;
}

int main(int argc, char **argv)
{
    sn_sim_options_t opt = {STDOUT_FILENO, -1, 0U, 1U};
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *val = (i + 1 < argc) ? argv[i + 1] : NULL;
        if ((strcmp(arg, "--duration-ms") == 0) && (val != NULL) && (parse_u32(val, &opt.duration_ms) == 0)) {
            ++i;
        } else if ((strcmp(arg, "--seed") == 0) && (val != NULL) && (parse_u32(val, &opt.seed) == 0)) {
            ++i;
        } else if ((strcmp(arg, "--input") == 0) && (val != NULL)) {
            opt.in_fd = open(val, O_RDONLY | O_NONBLOCK);
            if (opt.in_fd < 0) {
                perror(val);
                return EXIT_FAILURE;
            }
            ++i;
        } else if ((strcmp(arg, "--output") == 0) && (val != NULL)) {
            opt.out_fd = open(val, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (opt.out_fd < 0) {
                perror(val);
                return EXIT_FAILURE;
            }
            ++i;
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    board_sim_configure(&opt);
    board_init();
    sn_node_run();
    return EXIT_FAILURE;
}

void vApplicationIdleHook(void)
{
    static const struct timespec pause = {0, 1000000L};
    board_sim_idle();
    (void)nanosleep(&pause, NULL);
}

void sn_rtos_assert_failed(const char *file, int line)
{
    (void)fprintf(stderr, "[sim] assertion failed at %s:%d\n", file, line);
    abort();
}
