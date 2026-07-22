#!/usr/bin/env python3
"""Slice TER257 into 16x16 terrain tiles and export the Earth map from MAP.PIC.

Earth map (CivOne): MAP.PIC 320x200 X1 image; terrain layer is top-left 80x50
pixels whose values are map terrain codes (not Terrain enum). Converted to
CivOne Terrain enum ids for rendering:

  MAP byte -> Terrain enum (row in TER257 for base tile at x=0)
  default/1 -> Ocean (10)
  2 Forest (3), 3 Swamp (8), 6 Plains (1), 7 Tundra (6),
  9 River (11), 10 Grassland (2), 11 Jungle (9), 12 Hills (4),
  13 Mountains (5), 14 Desert (0), 15 Arctic (7)

Base tile blit for GFX256 without autotile: TER257[0, terrainId*16, 16, 16].

CivOne composites two layers:
  - LandBase  = SP257[0, 64, 16, 16]  (solid green under all land)
  - OceanBase = TER257[0, 160, 16, 16] (solid ocean under ocean tiles)
  - Terrain row from TER257 is often sparse/translucent detail drawn on top.
Exported base_XX_*.png tiles are pre-composited so the viewer can draw one layer.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from extract_civ_pics import decode_pic, lzw_decode, rle_decode  # noqa: E402

CIV = ROOT / "Redist" / "Data" / "CIV"
EXTRACT = ROOT / "Redist" / "Images" / "civ_extract"
TILES_OUT = ROOT / "Redist" / "Images" / "civ_tiles"
MAP_OUT = ROOT / "Redist" / "Images" / "civ_map"

TILE = 16
MAP_W, MAP_H = 80, 50

# Terrain enum ids that use the ocean base instead of the solid green land base.
OCEAN_TERRAIN_IDS = {10}  # Ocean (River uses land base under its pattern in full game)

TERRAIN_NAMES = {
    0: "desert",
    1: "plains",
    2: "grassland",
    3: "forest",
    4: "hills",
    5: "mountains",
    6: "tundra",
    7: "arctic",
    8: "swamp",
    9: "jungle",
    10: "ocean",
    11: "river",
}

# MAP.PIC pixel value -> Terrain enum id (CivOne Map.LoadSave)
MAP_BYTE_TO_TERRAIN = {
    2: 3,   # Forest
    3: 8,   # Swamp
    6: 1,   # Plains
    7: 6,   # Tundra
    9: 11,  # River
    10: 2,  # Grassland
    11: 9,  # Jungle
    12: 4,  # Hills
    13: 5,  # Mountains
    14: 0,  # Desert
    15: 7,  # Arctic
}


def load_map_layer(path: Path) -> bytes:
    data = path.read_bytes()
    index = 0
    while index < len(data) - 1:
        magic = struct.unpack_from("<H", data, index)[0]
        index += 2
        if magic == 0x304D:
            index += 2
            first, last = data[index], data[index + 1]
            index += 2
            for i in range(256):
                if first <= i <= last:
                    index += 3
        elif magic == 0x3045:
            index += 2
            first, last = data[index], data[index + 1]
            index += 2
            for i in range(256):
                if first <= i <= last:
                    index += 1
        elif magic in (0x3058, 0x3158):
            length = struct.unpack_from("<H", data, index)[0]
            index += 2
            w = struct.unpack_from("<H", data, index)[0]
            index += 2
            h = struct.unpack_from("<H", data, index)[0]
            index += 2
            index += 1  # bits
            compressed = data[index : index + length - 5]
            raw = rle_decode(lzw_decode(compressed))
            if magic == 0x3058:
                pic = bytearray(raw[: w * h])
            else:
                pic = bytearray(w * h)
                c = 0
                for y in range(h):
                    for x in range(0, w, 2):
                        if c >= len(raw):
                            break
                        pic[y * w + x] = raw[c] & 0x0F
                        if x + 1 < w:
                            pic[y * w + x + 1] = (raw[c] & 0xF0) >> 4
                        c += 1
            # Terrain layer: top-left MAP_W x MAP_H of the bitmap
            out = bytearray(MAP_W * MAP_H)
            for y in range(MAP_H):
                for x in range(MAP_W):
                    code = pic[y * w + x]
                    out[y * MAP_W + x] = MAP_BYTE_TO_TERRAIN.get(code, 10)  # Ocean default
            return bytes(out)
        else:
            break
    raise RuntimeError(f"No image layer in {path}")


def ensure_sheet(stem: str) -> Image.Image:
    """Load EXTRACT/{stem}_320x200.png, re-decoding from CIV if needed."""
    path = EXTRACT / f"{stem}_320x200.png"
    if not path.exists():
        pal, img, _ = decode_pic(CIV / f"{stem}.PIC")
        if img is None:
            raise SystemExit(f"{stem} missing — run extract_civ_pics.py first")
        EXTRACT.mkdir(parents=True, exist_ok=True)
        img.save(path)
    return Image.open(path).convert("RGBA")


def composite_base(underlay: Image.Image, overlay: Image.Image) -> Image.Image:
    """Draw sparse TER257 detail over solid land/ocean base (CivOne TileBase + layer)."""
    out = underlay.convert("RGBA").copy()
    return Image.alpha_composite(out, overlay.convert("RGBA"))


# Civ palette index 3 → (0,168,168) cyan. CivOne ColourReplace(3, 0) on specials/huts/units.
CYAN_KEY = (0, 168, 168, 255)


def clear_cyan_and_chrome(cell: Image.Image, clear_top_left: bool = True) -> Image.Image:
    """Make cyan transparent; optionally wipe top row + left column (unit/special chrome)."""
    cell = cell.convert("RGBA").copy()
    px = cell.load()
    w, h = cell.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if (r, g, b) == CYAN_KEY[:3] or (clear_top_left and (y == 0 or x == 0)):
                px[x, y] = (0, 0, 0, 0)
    return cell


def slice_ter257() -> None:
    sheet = ensure_sheet("TER257")
    sp257 = ensure_sheet("SP257")

    TILES_OUT.mkdir(parents=True, exist_ok=True)

    # CivOne bases: land green under land tiles, ocean under ocean tiles.
    land_base = sp257.crop((0, 4 * TILE, TILE, 5 * TILE))   # SP257[0, 64]
    ocean_base = sheet.crop((0, 10 * TILE, TILE, 11 * TILE))  # TER257[0, 160]
    land_base.save(TILES_OUT / "land_base.png")
    ocean_base.save(TILES_OUT / "ocean_base.png")
    print(f"  land_base  (SP257 0,64)  -> land_base.png")
    print(f"  ocean_base (TER257 0,160) -> ocean_base.png")

    # Full grid dump for tooling (raw cells, not pre-composited)
    cols, rows = sheet.size[0] // TILE, sheet.size[1] // TILE
    for r in range(rows):
        for c in range(cols):
            cell = sheet.crop((c * TILE, r * TILE, (c + 1) * TILE, (r + 1) * TILE))
            cell.save(TILES_OUT / f"ter_r{r:02d}_c{c:02d}.png")

    # Base tiles for the map viewer: column 0, composited onto land/ocean base.
    # (Column 0 = no matching-neighbor autotile; good default for a simple Earth view.)
    for tid, name in TERRAIN_NAMES.items():
        y = tid * TILE
        if y + TILE > sheet.size[1]:
            print(f"warn: no row for terrain {tid} {name}")
            continue
        if tid == 11:
            # Rivers come from SP257 row y=80 with a direction bitfield (N=1,E=2,S=4,W=8),
            # not TER257 row 11 (which is solid ocean blue).
            continue
        overlay = sheet.crop((0, y, TILE, y + TILE))
        under = ocean_base if tid in OCEAN_TERRAIN_IDS else land_base
        cell = composite_base(under, overlay)
        cell.save(TILES_OUT / f"base_{tid:02d}_{name}.png")
        print(f"  base tile {tid:2d} {name:12s} -> base_{tid:02d}_{name}.png  (over {'ocean' if tid in OCEAN_TERRAIN_IDS else 'land'})")

    # River direction tiles: SP257[dir*16, 80] over land base (CivOne GetRiverLayer).
    for d in range(16):
        overlay = sp257.crop((d * TILE, 5 * TILE, (d + 1) * TILE, 6 * TILE))
        cell = composite_base(land_base, overlay)
        cell.save(TILES_OUT / f"river_d{d:02d}.png")
    # Default base_11_river for tools that only look at base_XX (isolated river = dir 0).
    Image.open(TILES_OUT / "river_d00.png").save(TILES_OUT / "base_11_river.png")
    print(f"  river dirs 0..15 + base_11_river -> river_dXX.png (over land, SP257 y=80)")

    # Special resources: SP257[terrainId*16, 112] (CivOne GetSpecial + ColourReplace(3,0)).
    SPECIAL_NAMES = {
        0: "desert",    # Oasis
        1: "plains",    # Horses
        2: "grassland", # (row graphic; shield is separate)
        3: "forest",    # Game
        4: "hills",     # Coal
        5: "mountains", # Gold
        6: "tundra",    # Game
        7: "arctic",    # Seals
        8: "swamp",     # Oil
        9: "jungle",    # Gems
        10: "ocean",    # Fish
    }
    for tid, name in SPECIAL_NAMES.items():
        cell = sp257.crop((tid * TILE, 7 * TILE, (tid + 1) * TILE, 8 * TILE))
        cell = clear_cyan_and_chrome(cell, clear_top_left=True)
        cell.save(TILES_OUT / f"special_{tid:02d}_{name}.png")
        print(f"  special {tid:2d} {name:12s} -> special_{tid:02d}_{name}.png")

    # Grassland shield: SP257[152, 40, 8, 8] centered on transparent 16x16 (CivOne).
    shield = Image.new("RGBA", (TILE, TILE), (0, 0, 0, 0))
    icon = clear_cyan_and_chrome(sp257.crop((152, 40, 160, 48)), clear_top_left=False)
    shield.paste(icon, (4, 4), icon)
    shield.save(TILES_OUT / "special_shield.png")
    print("  special_shield (SP257 152,40 8x8) -> special_shield.png")

    # Goody hut: SP257[240, 112]
    hut = clear_cyan_and_chrome(sp257.crop((15 * TILE, 7 * TILE, 16 * TILE, 8 * TILE)), clear_top_left=True)
    hut.save(TILES_OUT / "special_hut.png")
    print("  special_hut (SP257 240,112) -> special_hut.png")

    # City map icons: SP257 15x15 at (193,113) building + (209,113) walls (Civ chrome inset).
    # Building silhouette is exported WHITE-on-transparent so the game can tint it
    # with ColourDark (CivOne: ColourReplace(5, ColourDark[owner])).
    city_icon_raw = clear_cyan_and_chrome(sp257.crop((193, 113, 208, 128)), clear_top_left=False)
    city_icon = Image.new("RGBA", city_icon_raw.size, (0, 0, 0, 0))
    src_px = city_icon_raw.load()
    dst_px = city_icon.load()
    for yy in range(city_icon_raw.size[1]):
        for xx in range(city_icon_raw.size[0]):
            r, g, b, a = src_px[xx, yy]
            if a and r < 40 and g < 40 and b < 40:
                dst_px[xx, yy] = (255, 255, 255, 255)
    city_walls = clear_cyan_and_chrome(sp257.crop((209, 113, 224, 128)), clear_top_left=False)
    city_icon.save(TILES_OUT / "city_icon.png")
    city_walls.save(TILES_OUT / "city_walls.png")
    print("  city_icon  (SP257 193,113 15x15 white silhouette) -> city_icon.png")
    print("  city_walls (SP257 209,113 15x15) -> city_walls.png")

    # Mouse pointer torch: SP257[113, 33, 11, 13] (palette index 0 = transparent).
    # Also written to Redist/Images/pointer.png for g_Cursor in Main.cpp.
    torch = sp257.crop((113, 33, 113 + 11, 33 + 13)).convert("RGBA")
    torch.save(TILES_OUT / "pointer_torch.png")
    pointer_out = ROOT / "Redist" / "Images" / "pointer.png"
    torch.save(pointer_out)
    print("  pointer torch (SP257 113,33 11x13) -> pointer_torch.png + Images/pointer.png")

    # Coast sheets for ocean/lake shores (CivOne GetOceanLayer).
    # TER257 y=176 holds 8x8 coast segments; SP299 holds 16x16 corner cases.
    sheet.save(TILES_OUT / "ter257_sheet.png")
    sp257.save(TILES_OUT / "sp257_sheet.png")
    sp299 = ensure_sheet("SP299")
    sp299.save(TILES_OUT / "sp299_sheet.png")
    # Pre-export SP299 corner tiles (cleaned).
    for i, x in enumerate((224, 240, 256, 272)):
        corner = clear_cyan_and_chrome(sp299.crop((x, 100, x + TILE, 116)), clear_top_left=False)
        # Also drop pure black chrome if any
        corner.save(TILES_OUT / f"coast_corner_{i}.png")
    print("  coast sheets ter257/sp257/sp299 + 4 SP299 corners")

    print(f"Sliced full grid {cols}x{rows} + bases + specials + coasts -> {TILES_OUT}")


def river_directions(layer: bytes, x: int, y: int) -> int:
    """CivOne river bitfield: N=1 E=2 S=4 W=8 if neighbor is river or ocean."""

    def at(xx: int, yy: int) -> int:
        if yy < 0 or yy >= MAP_H:
            return 10
        xx %= MAP_W
        return layer[yy * MAP_W + xx]

    def connects(xx: int, yy: int) -> bool:
        t = at(xx, yy)
        return t in (10, 11)

    dirs = 0
    if connects(x, y - 1):
        dirs |= 1
    if connects(x + 1, y):
        dirs |= 2
    if connects(x, y + 1):
        dirs |= 4
    if connects(x - 1, y):
        dirs |= 8
    return dirs


def export_earth() -> None:
    MAP_OUT.mkdir(parents=True, exist_ok=True)
    layer = load_map_layer(CIV / "MAP.PIC")
    bin_path = MAP_OUT / "earth_terrain.bin"
    bin_path.write_bytes(layer)
    print(f"Earth map {MAP_W}x{MAP_H} -> {bin_path} ({len(layer)} bytes)")

    # Colourized low-res overview
    colors = {
        0: (210, 180, 80),
        1: (200, 180, 100),
        2: (60, 160, 50),
        3: (20, 100, 20),
        4: (140, 110, 60),
        5: (110, 110, 110),
        6: (200, 200, 210),
        7: (240, 240, 250),
        8: (90, 90, 50),
        9: (30, 120, 40),
        10: (20, 40, 160),
        11: (40, 100, 200),
    }
    prev = Image.new("RGB", (MAP_W, MAP_H))
    px = prev.load()
    for y in range(MAP_H):
        for x in range(MAP_W):
            px[x, y] = colors.get(layer[y * MAP_W + x], (255, 0, 255))
    prev.resize((MAP_W * 4, MAP_H * 4), Image.NEAREST).save(MAP_OUT / "earth_preview.png")
    print(f"Preview -> {MAP_OUT / 'earth_preview.png'}")

    # Full tile render using pre-composited bases + river directions
    bases: dict[int, Image.Image] = {}
    for tid, name in TERRAIN_NAMES.items():
        path = TILES_OUT / f"base_{tid:02d}_{name}.png"
        if path.exists():
            bases[tid] = Image.open(path).convert("RGBA")
    rivers = []
    for d in range(16):
        p = TILES_OUT / f"river_d{d:02d}.png"
        rivers.append(Image.open(p).convert("RGBA") if p.exists() else bases.get(2))

    scale = 2
    out = Image.new("RGBA", (MAP_W * TILE * scale, MAP_H * TILE * scale))
    for y in range(MAP_H):
        for x in range(MAP_W):
            tid = layer[y * MAP_W + x]
            if tid == 11 and rivers:
                cell = rivers[river_directions(layer, x, y)]
            else:
                cell = bases.get(tid, bases.get(10))
            if cell is None:
                continue
            out.paste(cell.resize((TILE * scale, TILE * scale), Image.NEAREST),
                      (x * TILE * scale, y * TILE * scale))
    render_path = MAP_OUT / "earth_render_preview.png"
    out.save(render_path)
    print(f"Tile render -> {render_path}")


def main() -> None:
    slice_ter257()
    export_earth()
    print("Done.")


if __name__ == "__main__":
    main()
