#!/usr/bin/env python3
"""
Convert PNG images to C byte arrays (XBM/u8g2 format, LSB-first)
and vice versa.

Usage:
  # PNG -> C array (for pasting into m1_display_data.c)
  python tools/png_to_xbm_array.py img2c logo.png --name m1_logo_40x32 --width 40 --height 32

  # C array -> PNG (for editing)
  python tools/png_to_xbm_array.py c2img m1_csrc/m1_display_data.c --name m1_logo_40x32 --width 40 --height 32 --output logo.png --scale 20

  # Batch export all 3 logos
  python tools/png_to_xbm_array.py export-all --scale 20

  # Batch import all 3 logos (from PNGs in current dir)
  python tools/png_to_xbm_array.py import-all --dir .
"""

import argparse
import re
import sys
import os

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: pip install Pillow", file=sys.stderr)
    sys.exit(1)

# The 3 logo definitions in m1_display_data.c
LOGOS = [
    {"name": "menu_m1_icon_M1_logo_1", "width": 128, "height": 64, "file": "M1_logo_128x64.png"},
    {"name": "m1_logo_40x32",          "width": 40,  "height": 32, "file": "M1_logo_40x32.png"},
    {"name": "m1_logo_26x14",          "width": 26,  "height": 14, "file": "M1_logo_26x14.png"},
]

SRC_FILE = os.path.join(os.path.dirname(__file__), "..", "m1_csrc", "m1_display_data.c")


def extract_array(src_text, var_name):
    """Extract byte array from C source text."""
    pat = re.compile(
        r'const\s+uint8_t\s+' + re.escape(var_name) + r'\s*\[\]\s*=\s*\{([^}]+)\}',
        re.DOTALL,
    )
    m = pat.search(src_text)
    if not m:
        raise ValueError(f"Could not find array '{var_name}' in source")
    hex_vals = re.findall(r'0x([0-9a-fA-F]{2})', m.group(1))
    return bytes(int(v, 16) for v in hex_vals)


def xbm_to_image(data, width, height):
    """Convert XBM byte array (LSB-first, u8g2 format) to PIL Image.
    bit=1 → black pixel (foreground), bit=0 → white pixel (background).
    """
    img = Image.new('1', (width, height), 1)  # white background
    pixels = img.load()
    bytes_per_row = (width + 7) // 8
    for y in range(height):
        for x in range(width):
            byte_idx = y * bytes_per_row + (x // 8)
            bit_idx = x % 8  # LSB first
            if byte_idx < len(data) and (data[byte_idx] & (1 << bit_idx)):
                pixels[x, y] = 0  # black (foreground)
    return img


def image_to_xbm(img, width, height):
    """Convert PIL Image to XBM byte array (LSB-first, u8g2 format).
    Dark pixels (luminance < 128) → bit=1, light pixels → bit=0.
    """
    if img.size != (width, height):
        img = img.resize((width, height), Image.NEAREST)
    img = img.convert('L')  # grayscale
    pixels = img.load()
    bytes_per_row = (width + 7) // 8
    data = bytearray(bytes_per_row * height)
    for y in range(height):
        for x in range(width):
            if pixels[x, y] < 128:  # dark pixel = ON
                byte_idx = y * bytes_per_row + (x // 8)
                bit_idx = x % 8
                data[byte_idx] |= (1 << bit_idx)
    return bytes(data)


def format_c_array(data, var_name, width, height, bytes_per_line=16):
    """Format byte array as C source code."""
    lines = [f"// '{var_name}', {width}x{height}px"]
    lines.append(f"const uint8_t {var_name}[] = {{")
    for i in range(0, len(data), bytes_per_line):
        chunk = data[i:i + bytes_per_line]
        hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
        if i + bytes_per_line < len(data):
            hex_str += ","
        lines.append(f"\t{hex_str}")
    lines.append("};")
    return "\n".join(lines)


def cmd_img2c(args):
    """Convert PNG to C array."""
    img = Image.open(args.png)
    print(f"Input: {args.png} ({img.size[0]}x{img.size[1]})", file=sys.stderr)

    w = args.width or img.size[0]
    h = args.height or img.size[1]

    # Convert early to avoid decode errors with some 1-bit PNGs
    img = img.convert('L')

    if img.size != (w, h):
        # Check if it's a scaled version
        if img.size[0] % w == 0 and img.size[1] % h == 0:
            scale = img.size[0] // w
            print(f"Detected {scale}x scale, downsampling to {w}x{h}", file=sys.stderr)
        img = img.resize((w, h), Image.NEAREST)

    data = image_to_xbm(img, w, h)
    print(format_c_array(data, args.name, w, h))


def cmd_c2img(args):
    """Convert C array to PNG."""
    with open(args.source, "r") as f:
        src = f.read()
    data = extract_array(src, args.name)
    img = xbm_to_image(data, args.width, args.height)

    scale = args.scale or 1
    if scale > 1:
        img = img.resize((args.width * scale, args.height * scale), Image.NEAREST)

    out = args.output or f"{args.name}.png"
    img.save(out)
    print(f"Saved: {out} ({img.size[0]}x{img.size[1]})", file=sys.stderr)


def cmd_export_all(args):
    """Export all 3 logos as PNGs."""
    with open(SRC_FILE, "r") as f:
        src = f.read()

    scale = args.scale or 10
    outdir = args.dir or "."

    for logo in LOGOS:
        data = extract_array(src, logo["name"])
        img = xbm_to_image(data, logo["width"], logo["height"])

        # Save 1:1
        native_path = os.path.join(outdir, logo["file"].replace(".png", "_1x.png"))
        img.save(native_path)

        # Save scaled
        s = scale if logo["width"] < 128 else max(scale // 2, 1)
        scaled = img.resize((logo["width"] * s, logo["height"] * s), Image.NEAREST)
        scaled_path = os.path.join(outdir, logo["file"])
        scaled.save(scaled_path)

        print(f"  {logo['name']}: {scaled_path} ({scaled.size[0]}x{scaled.size[1]}) + {native_path}", file=sys.stderr)


def cmd_import_all(args):
    """Import all 3 edited PNGs back to C arrays. Prints the replacement code."""
    indir = args.dir or "."
    for logo in LOGOS:
        png_path = os.path.join(indir, logo["file"])
        if not os.path.exists(png_path):
            # Try _1x variant
            png_path = os.path.join(indir, logo["file"].replace(".png", "_1x.png"))
        if not os.path.exists(png_path):
            print(f"WARNING: {logo['file']} not found in {indir}, skipping", file=sys.stderr)
            continue

        img = Image.open(png_path)
        w, h = logo["width"], logo["height"]
        if img.size != (w, h):
            img = img.resize((w, h), Image.NEAREST)

        data = image_to_xbm(img, w, h)
        print(format_c_array(data, logo["name"], w, h))
        print()


def main():
    parser = argparse.ArgumentParser(description="PNG ↔ C byte array converter for M1 display bitmaps")
    sub = parser.add_subparsers(dest="command")

    p = sub.add_parser("img2c", help="Convert PNG to C byte array")
    p.add_argument("png", help="Input PNG file")
    p.add_argument("--name", required=True, help="C variable name")
    p.add_argument("--width", type=int, help="Target width (auto-detect if omitted)")
    p.add_argument("--height", type=int, help="Target height (auto-detect if omitted)")

    p = sub.add_parser("c2img", help="Convert C array to PNG")
    p.add_argument("source", help="C source file")
    p.add_argument("--name", required=True, help="C variable name")
    p.add_argument("--width", type=int, required=True)
    p.add_argument("--height", type=int, required=True)
    p.add_argument("--output", help="Output PNG path")
    p.add_argument("--scale", type=int, help="Scale factor for output")

    p = sub.add_parser("export-all", help="Export all 3 logos as PNGs")
    p.add_argument("--scale", type=int, default=20, help="Scale factor (default: 20)")
    p.add_argument("--dir", default=".", help="Output directory")

    p = sub.add_parser("import-all", help="Import all 3 edited PNGs, print C arrays")
    p.add_argument("--dir", default=".", help="Directory containing edited PNGs")

    args = parser.parse_args()
    if args.command == "img2c":
        cmd_img2c(args)
    elif args.command == "c2img":
        cmd_c2img(args)
    elif args.command == "export-all":
        cmd_export_all(args)
    elif args.command == "import-all":
        cmd_import_all(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
