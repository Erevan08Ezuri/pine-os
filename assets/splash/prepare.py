#!/usr/bin/env python3
"""Pack the approved artwork for the portrait Tab5 (requires Pillow).

Normal firmware builds use the committed BMP and do not require Pillow.
"""
from pathlib import Path
import struct
from PIL import Image, ImageOps

root = Path(__file__).resolve().parent
image = ImageOps.fit(Image.open(root / 'pineos-gold-source.png').convert('RGB'),
                     (720, 1280), method=Image.Resampling.LANCZOS)
# Windows BITMAPINFOHEADER + RGB565 masks. Positive height = bottom-up rows.
pixels = bytearray()
for y in range(image.height - 1, -1, -1):
    for x in range(image.width):
        r, g, b = image.getpixel((x, y))
        pixels += struct.pack('<H', ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
offset = 14 + 40 + 12
header = struct.pack('<2sIHHI', b'BM', offset + len(pixels), 0, 0, offset)
header += struct.pack('<IiiHHIIiiII', 40, 720, 1280, 1, 16, 3, len(pixels), 0, 0, 0, 0)
header += struct.pack('<III', 0xf800, 0x07e0, 0x001f)
(root / 'pineos-gold-tab5.bmp').write_bytes(header + pixels)
