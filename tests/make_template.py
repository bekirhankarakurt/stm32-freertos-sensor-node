#!/usr/bin/env python3
"""Write an unsigned slot image (header template + vectors + payload) like the linker produces."""
import struct
import sys

out, load = sys.argv[1], int(sys.argv[2], 0)
header = struct.pack("<IHHIIIBBHII", 0x4D494E53, 1, 0x200, load, 0, 0, 1, 0, 0, 0x12345678, 0)
blob = bytearray(header.ljust(0x200, b"\xff"))
blob += struct.pack("<II", 0x20017FE0, load + 0x200 + 0x41)
blob += bytes((i * 7) & 0xFF for i in range(3000))
open(out, "wb").write(blob)
