/**
 * @file main.c
 * @brief Application entry point on target.
 */
#include "FreeRTOS.h"

#include "board.h"
#include "node.h"

int main(void)
{
    board_init();
    sn_node_run();
    /* The scheduler only returns if it could not start. */
    board_reset();
}

void sn_rtos_assert_failed(const char *file, int line)
{
    (void)file;
    (void)line;
    board_reset();
}
