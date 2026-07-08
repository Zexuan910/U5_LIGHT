from __future__ import annotations

import struct
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
H750_IMAGES = Path(r"D:\WZX\IDE_workplace\touch\H750_TouchGFX\TouchGFX\assets\images")
USER_WATCH_BG = Path(r"C:\Users\35156\Downloads\ChatGPT Image 2026年7月8日 19_55_33.png")
OUT_DIR = ROOT / "Assets" / "extflash"
PACKAGE_C = ROOT / "User" / "UI" / "ui_asset_package.c"

ASSETS = [
    ("watch_bg", USER_WATCH_BG),
    ("sport_bg", H750_IMAGES / "sport_bg.png"),
]


def trim_near_black_border(image: Image.Image) -> Image.Image:
    rgb = image.convert("RGB")
    pixels = rgb.load()
    width, height = rgb.size
    threshold = 10
    left = width
    top = height
    right = -1
    bottom = -1

    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]
            if max(r, g, b) > threshold:
                if x < left:
                    left = x
                if x > right:
                    right = x
                if y < top:
                    top = y
                if y > bottom:
                    bottom = y

    if right < left or bottom < top:
        return rgb

    return rgb.crop((left, top, right + 1, bottom + 1))


def cover_240x280(path: Path) -> Image.Image:
    image = trim_near_black_border(Image.open(path))
    target_w = 240
    target_h = 280
    scale = max(target_w / image.width, target_h / image.height)
    resized = image.resize((int(image.width * scale + 0.5), int(image.height * scale + 0.5)), Image.Resampling.LANCZOS)
    left = (resized.width - target_w) // 2
    top = (resized.height - target_h) // 2
    return resized.crop((left, top, left + target_w, top + target_h)).convert("RGB")


def rgb565_bytes(path: Path) -> bytes:
    image = cover_240x280(path)

    out = bytearray()
    for r, g, b in image.getdata():
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out.extend(struct.pack(">H", value))
    return bytes(out)


def write_package_c(package: bytes) -> None:
    lines = [
        '#include "ui_asset_package.h"',
        "",
        "#if UI_ASSET_PROGRAMMER",
        f"const uint32_t g_ui_asset_package_size = {len(package)}UL;",
        "const uint8_t g_ui_asset_package[] = {",
    ]

    for offset in range(0, len(package), 16):
        chunk = package[offset:offset + 16]
        values = ", ".join(f"0x{value:02X}" for value in chunk)
        lines.append(f"    {values},")

    lines.extend([
        "};",
        "#endif",
        "",
    ])
    PACKAGE_C.write_text("\n".join(lines), encoding="ascii")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    payloads: list[tuple[str, bytes]] = []
    for name, path in ASSETS:
        data = rgb565_bytes(path)
        if len(data) != 240 * 280 * 2:
            raise ValueError(f"{name} converted to wrong size: {len(data)}")
        (OUT_DIR / f"{name}.rgb565.bin").write_bytes(data)
        payloads.append((name, data))

    header_size = 12 + (32 * len(payloads))
    offset = header_size
    header = bytearray()
    header.extend(b"U5UI")
    header.extend(struct.pack("<II", 1, len(payloads)))

    body = bytearray()
    for name, data in payloads:
        name_bytes = name.encode("ascii")
        header.extend(name_bytes[:16].ljust(16, b"\0"))
        header.extend(struct.pack("<IIII", offset, len(data), 240, 280))
        body.extend(data)
        offset += len(data)

    package = bytes(header + body)
    (OUT_DIR / "u5_ui_assets_extflash.bin").write_bytes(package)
    write_package_c(package)


if __name__ == "__main__":
    main()
