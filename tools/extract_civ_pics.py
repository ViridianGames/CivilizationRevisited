#!/usr/bin/env python3
"""Extract Sid Meier's Civilization (DOS) PIC/PAL graphics.

Format reverse-engineered by CivOne (CC0): https://github.com/Solen1985/CivOne

Chunks (little-endian magic as uint16):
  0x304D 'M0' — 256-colour VGA palette (6-bit RGB * 4)
  0x3045 'E0' — 4-bit colour dither table (optional)
  0x3058 'X0' — 8-bit image (LZW then RLE)
  0x3158 'X1' — 4-bit image (LZW then RLE, two pixels/byte)

Requires: Pillow
"""
from __future__ import annotations

import struct
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
CIV = ROOT / "Redist" / "Data" / "CIV"
OUT = ROOT / "Redist" / "Images" / "civ_extract"


def code_length(n: int) -> int:
    for i in range(31, -1, -1):
        if (n >> i) & 1:
            return i + 1
    return 1


def lzw_decode(data: bytes, min_bits: int = 8, max_bits: int = 11) -> bytes:
    """CivOne LZW.Decode (flushDictionary=true, clearEnd=false).

    Critical: when flushDictionary is true, missing codes (KwKwK) are always
    inserted even if the dictionary is 'full'; then the dict is reset when
    code length would exceed maxBits. Using `count < max` alone desyncs the
    bitstream and truncates/garbles the bottom of images.
    """

    def reset_dict():
        d = {i: bytes([i]) for i in range(1 << min_bits)}
        d[len(d)] = b""  # reserved end-code slot (value == 1<<min_bits)
        values = {",".join(map(str, v)) for v in d.values()}
        return d, values

    dictionary, values = reset_dict()
    out = bytearray()
    value = 0
    counter = 0
    entry = b""

    for i in range(len(data)):
        clen = code_length(len(dictionary))
        if clen > max_bits:
            clen = max_bits
        for bit in range(8):
            value |= ((data[i] >> bit) & 1) << counter
            counter += 1
            if counter != clen:
                continue

            # End code (clearEnd=false): 1 << min_bits
            if value == (1 << min_bits):
                return bytes(out)

            # KwKwK — always add when missing (flushDictionary implies no max gate)
            if value not in dictionary:
                bts = entry + entry[:1]
                dictionary[len(dictionary)] = bts
                values.add(",".join(map(str, bts)))

            out_val = dictionary[value]
            new_entry = entry + out_val[:1]
            out.extend(out_val)

            string_value = ",".join(map(str, new_entry))
            if string_value not in values:
                dictionary[len(dictionary)] = new_entry
                values.add(string_value)

            entry = out_val
            value = 0
            counter = 0

            if code_length(len(dictionary)) > max_bits:
                dictionary, values = reset_dict()
                entry = b""

    return bytes(out)


def rle_decode(data: bytes) -> bytes:
    """CivOne RLE: 0x90 is repeat marker; 0x90 0x00 is literal 0x90."""
    rle_repeat = 0x90
    rle_escape = 0x00
    out = bytearray()
    if not data:
        return b""
    value = data[0]
    i = 0
    while i < len(data):
        if data[i] != rle_repeat or (i + 1 < len(data) and data[i + 1] == rle_escape):
            value = data[i]
            out.append(value)
            if data[i] == rle_repeat and i + 1 < len(data) and data[i + 1] == rle_escape:
                i += 1
            i += 1
            continue
        if i + 1 >= len(data):
            break
        repeat = data[i + 1]
        # already wrote one `value`; write (repeat - 1) more
        out.extend([value] * (repeat - 1))
        i += 2
    return bytes(out)


def load_standalone_palette(path: Path) -> list[tuple[int, int, int]]:
    data = path.read_bytes()
    index = 0
    magic = struct.unpack_from("<H", data, index)[0]
    index += 2
    if magic != 0x304D:
        raise ValueError(f"{path.name}: not an M0 palette")
    index += 2  # length
    first = data[index]
    last = data[index + 1]
    index += 2
    palette = [(0, 0, 0)] * 256
    for i in range(256):
        if first <= i <= last:
            r, g, b = data[index], data[index + 1], data[index + 2]
            index += 3
            palette[i] = (min(255, r * 4), min(255, g * 4), min(255, b * 4))
    return palette


def decode_pic(
    path: Path,
    fallback_palette: list[tuple[int, int, int]] | None = None,
) -> tuple[list[tuple[int, int, int]] | None, Image.Image | None, dict]:
    data = path.read_bytes()
    index = 0
    palette: list[tuple[int, int, int]] | None = None
    picture: bytearray | None = None
    width = height = 0
    meta: dict = {"chunks": []}

    while index < len(data) - 1:
        magic = struct.unpack_from("<H", data, index)[0]
        index += 2
        if magic == 0x304D:  # M0 palette
            index += 2  # length
            first = data[index]
            last = data[index + 1]
            index += 2
            palette = [(0, 0, 0)] * 256
            for i in range(256):
                if i < first or i > last:
                    continue
                r, g, b = data[index], data[index + 1], data[index + 2]
                index += 3
                palette[i] = (min(255, r * 4), min(255, g * 4), min(255, b * 4))
            meta["chunks"].append("M0_palette")
        elif magic == 0x3045:  # E0 colour table
            index += 2  # length
            first = data[index]
            last = data[index + 1]
            index += 2
            for i in range(256):
                if first <= i <= last:
                    index += 1
            meta["chunks"].append("E0_table")
        elif magic in (0x3058, 0x3158):  # X0 8-bit / X1 4-bit
            length = struct.unpack_from("<H", data, index)[0]
            index += 2
            width = struct.unpack_from("<H", data, index)[0]
            index += 2
            height = struct.unpack_from("<H", data, index)[0]
            index += 2
            bits = data[index]
            index += 1
            compressed = data[index : index + length - 5]
            index += length - 5
            raw = rle_decode(lzw_decode(compressed))
            meta["bits"] = bits
            if magic == 0x3058:
                picture = bytearray(raw[: width * height])
                if len(picture) < width * height:
                    picture.extend([0] * (width * height - len(picture)))
                meta["chunks"].append(f"X0_{width}x{height}")
            else:
                picture = bytearray(width * height)
                c = 0
                for y in range(height):
                    for x in range(0, width, 2):
                        if c >= len(raw):
                            break
                        picture[y * width + x] = raw[c] & 0x0F
                        if x + 1 < width:
                            picture[y * width + x + 1] = (raw[c] & 0xF0) >> 4
                        c += 1
                meta["chunks"].append(f"X1_{width}x{height}")
        else:
            meta["chunks"].append(f"unknown_{magic:#x}@stop")
            break

    if picture is None or width == 0:
        return palette, None, meta

    if palette is None:
        palette = fallback_palette or [(i, i, i) for i in range(256)]

    img = Image.new("RGBA", (width, height))
    px = img.load()
    for y in range(height):
        for x in range(width):
            idx = picture[y * width + x]
            if idx == 0:
                px[x, y] = (0, 0, 0, 0)
            else:
                r, g, b = palette[idx]
                px[x, y] = (r, g, b, 255)
    return palette, img, meta


def main() -> None:
    if not CIV.is_dir():
        raise SystemExit(f"Missing {CIV}")
    OUT.mkdir(parents=True, exist_ok=True)

    fallback = None
    sp256 = CIV / "SP256.PAL"
    if sp256.exists():
        try:
            fallback = load_standalone_palette(sp256)
        except Exception as e:
            print(f"warn: SP256.PAL: {e}")

    pics = sorted(CIV.glob("*.PIC")) + sorted(CIV.glob("*.pic"))
    ok = fail = 0
    for path in pics:
        try:
            pal, img, meta = decode_pic(path, fallback_palette=fallback)
        except Exception as e:
            print(f"FAIL {path.name}: {e}")
            fail += 1
            continue
        if img is None:
            print(f"SKIP {path.name}: no image {meta}")
            fail += 1
            continue
        out = OUT / f"{path.stem}_{img.size[0]}x{img.size[1]}.png"
        img.save(out)
        print(f"OK   {path.name:16s} -> {out.name:28s} chunks={meta['chunks']}")
        ok += 1

    for path in sorted(CIV.glob("*.PAL")):
        try:
            pal = load_standalone_palette(path)
        except Exception:
            continue
        strip = Image.new("RGB", (256, 16))
        sp = strip.load()
        for i, (r, g, b) in enumerate(pal):
            for y in range(16):
                sp[i, y] = (r, g, b)
        strip.save(OUT / f"{path.stem}_palette.png")

    print(f"\nDone: {ok} images, {fail} failed/skipped -> {OUT}")


if __name__ == "__main__":
    main()
