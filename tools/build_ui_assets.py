from __future__ import annotations

import struct
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
H750_IMAGES = Path(r"D:\WZX\IDE_workplace\touch\H750_TouchGFX\TouchGFX\assets\images")
OUT_DIR = ROOT / "Assets" / "extflash"

ASSETS = [
    ("watch_bg", H750_IMAGES / "watch_bg.png"),
    ("sport_bg", H750_IMAGES / "sport_bg.png"),
]


def rgb565_bytes(path: Path) -> bytes:
    image = Image.open(path).convert("RGB")
    if image.size != (240, 280):
        raise ValueError(f"{path} must be 240x280, got {image.size}")

    out = bytearray()
    for r, g, b in image.getdata():
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out.extend(struct.pack(">H", value))
    return bytes(out)


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

    (OUT_DIR / "u5_ui_assets_extflash.bin").write_bytes(bytes(header + body))


if __name__ == "__main__":
    main()
