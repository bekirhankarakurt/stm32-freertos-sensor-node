/**
 * @file cxx_support.cpp
 * @brief Minimal C++ runtime hooks for a freestanding, exception-free image.
 */
#include "board.h"

/** Called if a pure virtual function is ever invoked (programming error). */
extern "C" void __cxa_pure_virtual()
{
    board_reset();
}
