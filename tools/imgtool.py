#!/usr/bin/env python3
"""Image tool for the sensor-node bootloader.

sign   Patch size, payload CRC-32 and header CRC-32 into a linked slot image.
merge  Combine binaries into one flash image (gaps filled with 0xFF).
info   Print and verify the header of a signed image.
"""
import argparse
import struct
import sys
import zlib

MAGIC = 0x4D494E53  # "SNIM"
HEADER_VERSION = 1
HEADER_AREA = 0x200
HEADER_FMT = "<IHHIIIBBHI"  # everything up to (excluding) header_crc32
HEADER_LEN = struct.calcsize(HEADER_FMT) + 4


def parse_header(blob):
    if len(blob) < HEADER_AREA + 8:
        raise ValueError("image smaller than the header area")
    fields = struct.unpack_from(HEADER_FMT, blob, 0)
    (header_crc,) = struct.unpack_from("<I", blob, HEADER_LEN - 4)
    keys = ("magic", "header_version", "header_area", "load_addr", "image_size", "image_crc32",
            "ver_major", "ver_minor", "ver_patch", "git_hash")
    hdr = dict(zip(keys, fields))
    hdr["header_crc32"] = header_crc
    return hdr


def cmd_sign(args):
    blob = bytearray(open(args.input, "rb").read())
    hdr = parse_header(blob)
    if hdr["magic"] != MAGIC or hdr["header_area"] != HEADER_AREA:
        sys.exit(f"{args.input}: no image header template found at offset 0")
    payload = bytes(blob[HEADER_AREA:])
    hdr["image_size"] = len(payload)
    hdr["image_crc32"] = zlib.crc32(payload) & 0xFFFFFFFF
    packed = struct.pack(HEADER_FMT, hdr["magic"], HEADER_VERSION, HEADER_AREA, hdr["load_addr"],
                         hdr["image_size"], hdr["image_crc32"], hdr["ver_major"], hdr["ver_minor"],
                         hdr["ver_patch"], hdr["git_hash"])
    blob[0:HEADER_LEN] = packed + struct.pack("<I", zlib.crc32(packed) & 0xFFFFFFFF)
    with open(args.output, "wb") as f:
        f.write(blob)
    if args.header_out:
        with open(args.header_out, "wb") as f:
            f.write(blob[:HEADER_AREA])
    print(f"signed {args.output}: load=0x{hdr['load_addr']:08X} size={hdr['image_size']} "
          f"crc32=0x{hdr['image_crc32']:08X}")


def cmd_info(args):
    blob = open(args.input, "rb").read()
    hdr = parse_header(blob)
    ok = (hdr["magic"] == MAGIC
          and zlib.crc32(blob[:HEADER_LEN - 4]) & 0xFFFFFFFF == hdr["header_crc32"]
          and hdr["image_size"] == len(blob) - HEADER_AREA
          and zlib.crc32(blob[HEADER_AREA:]) & 0xFFFFFFFF == hdr["image_crc32"])
    print(f"version   {hdr['ver_major']}.{hdr['ver_minor']}.{hdr['ver_patch']} (git {hdr['git_hash']:08x})")
    print(f"load addr 0x{hdr['load_addr']:08X}")
    print(f"size      {hdr['image_size']} bytes")
    print(f"crc32     0x{hdr['image_crc32']:08X}")
    print(f"valid     {'yes' if ok else 'NO'}")
    return 0 if ok else 1


def cmd_merge(args):
    base = int(args.base, 0)
    parts = []
    for spec in args.parts:
        path, _, addr = spec.partition("@")
        parts.append((int(addr, 0), open(path, "rb").read(), path))
    end = max(addr + len(data) for addr, data, _ in parts)
    out = bytearray(b"\xff" * (end - base))
    for addr, data, path in sorted(parts):
        off = addr - base
        if off < 0 or any(b != 0xFF for b in out[off:off + len(data)]):
            sys.exit(f"{path}: overlaps another part or lies below the base address")
        out[off:off + len(data)] = data
    with open(args.output, "wb") as f:
        f.write(out)
    print(f"merged {len(parts)} part(s) into {args.output} ({len(out)} bytes)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("sign", help="finalise an image header")
    s.add_argument("input")
    s.add_argument("output")
    s.add_argument("--header-out", help="also write the 512-byte header area")
    s.set_defaults(func=cmd_sign)
    i = sub.add_parser("info", help="print and verify an image header")
    i.add_argument("input")
    i.set_defaults(func=cmd_info)
    m = sub.add_parser("merge", help="combine binaries into one flash image")
    m.add_argument("output")
    m.add_argument("parts", nargs="+", help="FILE@ADDRESS")
    m.add_argument("--base", default="0x08000000")
    m.set_defaults(func=cmd_merge)
    args = ap.parse_args()
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
