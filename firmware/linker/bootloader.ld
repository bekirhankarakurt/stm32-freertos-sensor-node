/* Bootloader: sector 0 (16 KiB). */
MEMORY
{
    FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 16K
    RAM    (rwx) : ORIGIN = 0x20000000, LENGTH = 96K - 32
    SHARED (rw)  : ORIGIN = 0x20017FE0, LENGTH = 32
}
ENTRY(Reset_Handler)
_min_stack = 0x800;
INCLUDE sections.ld.inc
