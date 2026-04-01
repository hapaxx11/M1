#!/usr/bin/env python3
"""
png_to_xbm_array.py — Bidirectional PNG ↔ u8g2 XBM byte array converter.

Converts between 1-bit monochrome PNG images and C byte arrays compatible
with the u8g2 display library (LSB-first / x-swap format).

Usage:
    # Convert PNG to C array
    python png_to_xbm_array.py img2c logo.png --name menu_m1_icon_M1_logo_1 --width 128 --height 64

    # Convert PNG with scale factor (image is downsampled)
    python png_to_xbm_array.py img2c logo_10x.png --name m1_logo --width 128 --height 64

    # Export all logos from m1_display_data.c as scaled PNGs
    python png_to_xbm_array.py export-all --scale 10

    # Export a single array to PNG
    python png_to_xbm_array.py c2img --source m1_csrc/m1_display_data.c --name m1_logo_40x32 --width 40 --height 32 --scale 10

Polarity: dark pixel (value 0) in PNG → bit SET (1) in the C array.
This matches u8g2 OLED convention: bit 1 = pixel ON (white on black).

Byte format: LSB-first within each byte (u8g2 "x-swap" mode).
Row-major, left-to-right, top-to-bottom.
"""

import argparse
import base64
import io
import re
import struct
import sys
import zlib


def decode_png_1bit(data):
    """Decode a 1-bit grayscale PNG to a 2D pixel array.

    Returns (width, height, pixels) where pixels[y][x] is 0 (black) or 1 (white).
    Handles corrupted zlib checksums gracefully.
    """
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError("Not a valid PNG file")

    pos = 8
    width = height = None
    idat_chunks = []

    while pos + 8 <= len(data):
        length = struct.unpack('>I', data[pos:pos + 4])[0]
        chunk_type = data[pos + 4:pos + 8]
        end = min(pos + 8 + length, len(data))
        chunk_data = data[pos + 8:end]

        if chunk_type == b'IHDR':
            width = struct.unpack('>I', chunk_data[0:4])[0]
            height = struct.unpack('>I', chunk_data[4:8])[0]
            bit_depth = chunk_data[8]
            color_type = chunk_data[9]
            if bit_depth != 1 or color_type != 0:
                raise ValueError(
                    f"Expected 1-bit grayscale PNG, got depth={bit_depth} "
                    f"color_type={color_type}"
                )
        elif chunk_type == b'IDAT':
            idat_chunks.append(chunk_data)
        elif chunk_type == b'IEND':
            break

        pos += 12 + length
        if pos > len(data):
            break

    if width is None or height is None:
        raise ValueError("No IHDR chunk found in PNG")

    combined = b''.join(idat_chunks)

    # Try multiple decompression strategies to handle checksum errors
    raw = None
    strategies = [
        ("standard", lambda d: zlib.decompress(d)),
        ("decompressobj", lambda d: _decompress_obj(d)),
        ("raw_deflate", lambda d: zlib.decompressobj(-15).decompress(d[2:])),
    ]

    for name, strat in strategies:
        try:
            raw = strat(combined)
            break
        except zlib.error:
            continue

    if raw is None:
        raise ValueError("Failed to decompress PNG IDAT data")

    bytes_per_row = (width + 7) // 8
    stride = 1 + bytes_per_row
    available_rows = min(height, len(raw) // stride)

    if available_rows < height:
        print(
            f"  Warning: only {available_rows}/{height} rows decompressed "
            f"(PNG data may be truncated)",
            file=sys.stderr,
        )

    pixels = []
    prev_row = bytearray(bytes_per_row)

    for y in range(available_rows):
        offset = y * stride
        filter_type = raw[offset]
        row_data = bytearray(raw[offset + 1:offset + 1 + bytes_per_row])

        # Apply PNG row filter
        if filter_type == 1:  # Sub
            for i in range(1, len(row_data)):
                row_data[i] = (row_data[i] + row_data[i - 1]) & 0xFF
        elif filter_type == 2:  # Up
            for i in range(len(row_data)):
                row_data[i] = (row_data[i] + prev_row[i]) & 0xFF
        elif filter_type == 3:  # Average
            for i in range(len(row_data)):
                a = row_data[i - 1] if i > 0 else 0
                row_data[i] = (row_data[i] + (a + prev_row[i]) // 2) & 0xFF
        elif filter_type == 4:  # Paeth
            for i in range(len(row_data)):
                a = row_data[i - 1] if i > 0 else 0
                b = prev_row[i]
                c = prev_row[i - 1] if i > 0 else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                if pa <= pb and pa <= pc:
                    pr = a
                elif pb <= pc:
                    pr = b
                else:
                    pr = c
                row_data[i] = (row_data[i] + pr) & 0xFF

        row_pixels = []
        for x in range(width):
            byte_idx = x // 8
            bit_idx = 7 - (x % 8)  # MSB first in PNG
            row_pixels.append((row_data[byte_idx] >> bit_idx) & 1)

        pixels.append(row_pixels)
        prev_row = row_data

    # Pad missing rows with white (1)
    for _ in range(available_rows, height):
        pixels.append([1] * width)

    return width, height, pixels


def _decompress_obj(data):
    """Decompress with decompressobj, tolerating checksum errors."""
    obj = zlib.decompressobj()
    result = obj.decompress(data)
    try:
        result += obj.flush()
    except zlib.error:
        pass  # Checksum error at end — data is likely complete
    return result


def pixels_to_xbm(pixels, target_w, target_h, scale=1):
    """Convert pixel array to u8g2 XBM byte array (LSB-first / x-swap).

    Dark pixel (0) in source → bit SET (1) in output (inverted polarity).
    If scale > 1, samples from center of each scale block.
    """
    bytes_per_row = (target_w + 7) // 8
    result = []

    for y in range(target_h):
        for bx in range(bytes_per_row):
            byte_val = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x >= target_w:
                    break

                sy = y * scale + scale // 2
                sx = x * scale + scale // 2

                if sy < len(pixels) and sx < len(pixels[0]):
                    px = pixels[sy][sx]
                else:
                    px = 1  # white/background

                # Inverted: dark (0) → bit set (1)
                if px == 0:
                    byte_val |= (1 << bit)

            result.append(byte_val)

    return result


def xbm_to_pixels(xbm_bytes, width, height):
    """Convert u8g2 XBM byte array back to pixel array.

    Bit SET (1) → dark pixel (0) in output (inverted polarity).
    """
    bytes_per_row = (width + 7) // 8
    pixels = []

    for y in range(height):
        row = []
        for x in range(width):
            bx = x // 8
            bit = x % 8
            byte_val = xbm_bytes[y * bytes_per_row + bx]
            # Inverted: bit set (1) → dark pixel (0)
            row.append(0 if (byte_val >> bit) & 1 else 1)
        pixels.append(row)

    return pixels


def format_c_array(name, byte_data, bytes_per_line=16):
    """Format byte data as a C array definition."""
    lines = []
    for i in range(0, len(byte_data), bytes_per_line):
        chunk = byte_data[i:i + bytes_per_line]
        hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
        if i + bytes_per_line < len(byte_data):
            lines.append(f"\t{hex_vals},")
        else:
            lines.append(f"\t{hex_vals}")

    body = "\n".join(lines)
    return f"const uint8_t {name}[] = {{\n{body}\n}};"


def extract_c_array(source_text, name):
    """Extract byte values from a C array in source text."""
    pattern = rf'const uint8_t {re.escape(name)}\[\] = \{{([^}}]+)\}}'
    match = re.search(pattern, source_text)
    if not match:
        raise ValueError(f"Array '{name}' not found in source")
    values = []
    for token in match.group(1).split(','):
        token = token.strip()
        if token.startswith('0x'):
            values.append(int(token, 16))
    return values


def create_png_1bit(pixels, width, height, scale=1):
    """Create a 1-bit grayscale PNG from pixel array, optionally scaled up."""
    sw = width * scale
    sh = height * scale

    def make_chunk(chunk_type, data):
        crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
        return struct.pack('>I', len(data)) + chunk_type + data + struct.pack('>I', crc)

    # IHDR
    ihdr_data = struct.pack('>IIBBBBB', sw, sh, 1, 0, 0, 0, 0)
    ihdr = make_chunk(b'IHDR', ihdr_data)

    # IDAT — build raw scanlines
    raw_lines = []
    for y in range(sh):
        src_y = y // scale
        bytes_per_row = (sw + 7) // 8
        row_bytes = bytearray(bytes_per_row)
        for x in range(sw):
            src_x = x // scale
            px = pixels[src_y][src_x] if src_y < len(pixels) and src_x < len(pixels[0]) else 1
            if px == 0:  # dark pixel → bit set in PNG (value 0 = black)
                byte_idx = x // 8
                bit_idx = 7 - (x % 8)
                row_bytes[byte_idx] |= (0 << bit_idx)  # 0 = black in PNG
            else:
                byte_idx = x // 8
                bit_idx = 7 - (x % 8)
                row_bytes[byte_idx] |= (1 << bit_idx)  # 1 = white in PNG
        # Wait, this logic is wrong. Let me redo it.
        # In 1-bit PNG: 0 = black, 1 = white
        # We start with all zeros (black), set white bits
        row_bytes = bytearray(bytes_per_row)  # all zeros = all black
        for x in range(sw):
            src_x = x // scale
            px = pixels[src_y][src_x] if src_y < len(pixels) and src_x < len(pixels[0]) else 1
            if px == 1:  # white pixel
                byte_idx = x // 8
                bit_idx = 7 - (x % 8)
                row_bytes[byte_idx] |= (1 << bit_idx)

        raw_lines.append(b'\x00' + bytes(row_bytes))  # filter type 0 (None)

    compressed = zlib.compress(b''.join(raw_lines))
    idat = make_chunk(b'IDAT', compressed)

    # IEND
    iend = make_chunk(b'IEND', b'')

    # Assemble PNG
    signature = b'\x89PNG\r\n\x1a\n'
    return signature + ihdr + idat + iend


def cmd_img2c(args):
    """Convert PNG image to C array."""
    with open(args.png_file, 'rb') as f:
        data = f.read()

    src_w, src_h, pixels = decode_png_1bit(data)

    # Determine scale factor
    if src_w == args.width and src_h == args.height:
        scale = 1
    else:
        scale_x = src_w // args.width
        scale_y = src_h // args.height
        if scale_x != scale_y or scale_x < 1:
            print(
                f"Error: PNG is {src_w}x{src_h}, target is {args.width}x{args.height}. "
                f"Scale factors don't match ({scale_x}x vs {scale_y}x).",
                file=sys.stderr,
            )
            sys.exit(1)
        scale = scale_x
        print(f"Downsampling {src_w}x{src_h} → {args.width}x{args.height} (scale {scale}x)")

    xbm = pixels_to_xbm(pixels, args.width, args.height, scale)
    print(format_c_array(args.name, xbm))


def cmd_c2img(args):
    """Export C array to PNG image."""
    with open(args.source, 'r') as f:
        source = f.read()

    xbm = extract_c_array(source, args.name)
    pixels = xbm_to_pixels(xbm, args.width, args.height)
    png_data = create_png_1bit(pixels, args.width, args.height, args.scale)

    out_name = args.output or f"{args.name}.png"
    with open(out_name, 'wb') as f:
        f.write(png_data)
    print(f"Exported {args.name} ({args.width}x{args.height}) → {out_name} (scale {args.scale}x)")


def cmd_export_all(args):
    """Export all three M1 logo arrays as PNGs."""
    source_path = args.source or 'm1_csrc/m1_display_data.c'
    with open(source_path, 'r') as f:
        source = f.read()

    logos = [
        ('menu_m1_icon_M1_logo_1', 128, 64),
        ('m1_logo_26x14', 26, 14),
        ('m1_logo_40x32', 40, 32),
    ]

    for name, w, h in logos:
        try:
            xbm = extract_c_array(source, name)
            pixels = xbm_to_pixels(xbm, w, h)
            png_data = create_png_1bit(pixels, w, h, args.scale)
            out_name = f"M1_logo_{w}x{h}.png"
            with open(out_name, 'wb') as f:
                f.write(png_data)
            print(f"  {name} → {out_name} ({w * args.scale}x{h * args.scale}px)")
        except ValueError as e:
            print(f"  {name}: {e}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description='PNG ↔ u8g2 XBM byte array converter'
    )
    sub = parser.add_subparsers(dest='command', required=True)

    # img2c
    p_img2c = sub.add_parser('img2c', help='Convert PNG to C array')
    p_img2c.add_argument('png_file', help='Input PNG file')
    p_img2c.add_argument('--name', required=True, help='C array name')
    p_img2c.add_argument('--width', type=int, required=True, help='Target width in pixels')
    p_img2c.add_argument('--height', type=int, required=True, help='Target height in pixels')

    # c2img
    p_c2img = sub.add_parser('c2img', help='Export C array to PNG')
    p_c2img.add_argument('--source', required=True, help='Source C file')
    p_c2img.add_argument('--name', required=True, help='C array name')
    p_c2img.add_argument('--width', type=int, required=True, help='Image width')
    p_c2img.add_argument('--height', type=int, required=True, help='Image height')
    p_c2img.add_argument('--scale', type=int, default=1, help='Scale factor for output')
    p_c2img.add_argument('--output', help='Output PNG file name')

    # export-all
    p_all = sub.add_parser('export-all', help='Export all M1 logos as PNGs')
    p_all.add_argument('--scale', type=int, default=10, help='Scale factor')
    p_all.add_argument('--source', help='Source C file (default: m1_csrc/m1_display_data.c)')

    args = parser.parse_args()

    if args.command == 'img2c':
        cmd_img2c(args)
    elif args.command == 'c2img':
        cmd_c2img(args)
    elif args.command == 'export-all':
        cmd_export_all(args)


if __name__ == '__main__':
    main()
