"""Make Nethermost's program icons from platform/icon/nethermost-icon.png:
the Windows icon (platform/icon/derp.ico), the window icon on Linux
(platform/icon/derp.png, 256 pixels) and the Mac app icon (platform/icon/derp.icns).
Usage: py scripts/make-icons.py   (standard library only)

The picture fills the square on Windows and Linux. On the Mac it sits on a
rounded tile inside the margin macOS icons keep (824 of 1024 pixels), so it
lines up with the other apps in the Dock. Smaller sizes average the pixels
each one covers; sizes above the picture's own repeat its pixels."""
import pathlib
import struct
import zlib

FOLDER = pathlib.Path(__file__).resolve().parent.parent / 'platform' / 'icon'
SOURCE = FOLDER / 'nethermost-icon.png'
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
# The Mac's sizes and their entry types in an .icns file.
ICNS_TYPES = [(b'ic11', 32), (b'ic12', 64), (b'ic07', 128), (b'ic08', 256), (b'ic13', 512),
              (b'ic09', 512), (b'ic14', 1024), (b'ic10', 1024)]
MAC_TILE, MAC_RADIUS = 824 / 1024, 185 / 1024


def read_png(path):
    """Width, height and RGBA pixels (a list of rows of tuples) of an 8-bit RGB or RGBA PNG."""
    data = path.read_bytes()
    pos, idat = 8, b''
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack('>IIBBBBB', body)
            if depth != 8 or color not in (2, 6) or interlace:
                raise SystemExit(f'{path.name}: expected 8-bit RGB or RGBA, not interlaced')
            bpp = 4 if color == 6 else 3
        elif kind == b'IDAT':
            idat += body
        pos += 12 + length
    raw, stride = zlib.decompress(idat), width * bpp
    rows, prev = [], bytearray(stride)
    for y in range(height):
        line = raw[y * (stride + 1):(y + 1) * (stride + 1)]
        kind, row = line[0], bytearray(line[1:])
        for i in range(stride):
            a = row[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if kind == 1:
                row[i] = (row[i] + a) & 255
            elif kind == 2:
                row[i] = (row[i] + b) & 255
            elif kind == 3:
                row[i] = (row[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[i] = (row[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(row[x * bpp:x * bpp + 3]) + ((row[x * bpp + 3],) if bpp == 4 else (255,))
                     for x in range(width)])
        prev = row
    return width, height, rows


def png_bytes(size, rows):
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    raw = b''.join(b'\0' + b''.join(bytes(p) for p in row) for row in rows)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def resize(source, size):
    """The picture at size by size: each pixel the area-weighted average of the
    source pixels it covers (premultiplied by alpha), or the nearest one when enlarging."""
    width, height, rows = source
    sx, sy = width / size, height / size
    out = []
    for y in range(size):
        y0, y1 = y * sy, (y + 1) * sy
        row = []
        for x in range(size):
            x0, x1 = x * sx, (x + 1) * sx
            if sx <= 1:
                row.append(rows[min(height - 1, int(y0 + sy / 2))][min(width - 1, int(x0 + sx / 2))])
                continue
            total = [0.0, 0.0, 0.0, 0.0]
            weight = 0.0
            for j in range(int(y0), min(height, int(y1 + 0.999999))):
                wy = min(y1, j + 1) - max(y0, j)
                for i in range(int(x0), min(width, int(x1 + 0.999999))):
                    w = (min(x1, i + 1) - max(x0, i)) * wy
                    r, g, b, a = rows[j][i]
                    total[0] += r * a * w; total[1] += g * a * w; total[2] += b * a * w; total[3] += a * w
                    weight += w
            a = total[3] / weight
            row.append((0, 0, 0, 0) if a <= 0 else
                       tuple(round(min(255, total[c] / total[3])) for c in range(3)) + (round(a),))
        out.append(row)
    return out


def mac_tile(source, size):
    """The Mac icon at size: the picture shrunk to the tile, its corners rounded, on transparency."""
    tile = round(size * MAC_TILE)
    art, margin, radius = resize(source, tile), (size - round(size * MAC_TILE)) // 2, size * MAC_RADIUS
    rows = [[(0, 0, 0, 0)] * size for _ in range(size)]
    for y in range(tile):
        for x in range(tile):
            # Coverage of the rounded square at this pixel, sampled 4x4 for a smooth edge.
            inside = 0
            for j in range(4):
                for i in range(4):
                    px, py = x + (i + 0.5) / 4, y + (j + 0.5) / 4
                    cx = min(max(px, radius), tile - radius)
                    cy = min(max(py, radius), tile - radius)
                    inside += (px - cx) ** 2 + (py - cy) ** 2 <= radius * radius
            r, g, b, a = art[y][x]
            rows[margin + y][margin + x] = (r, g, b, round(a * inside / 16))
    return rows


def main():
    source = read_png(SOURCE)
    images = {size: png_bytes(size, resize(source, size)) for size in ICO_SIZES}
    header = struct.pack('<HHH', 0, 1, len(ICO_SIZES))
    entries, payload, offset = b'', b'', 6 + 16 * len(ICO_SIZES)
    for size in ICO_SIZES:
        data = images[size]
        entries += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
        payload += data
        offset += len(data)
    (FOLDER / 'derp.ico').write_bytes(header + entries + payload)
    (FOLDER / 'derp.png').write_bytes(images[256])
    tiles = {}
    body = b''
    for kind, size in ICNS_TYPES:
        if size not in tiles:
            tiles[size] = png_bytes(size, mac_tile(source, size))
        body += kind + struct.pack('>I', 8 + len(tiles[size])) + tiles[size]
    (FOLDER / 'derp.icns').write_bytes(b'icns' + struct.pack('>I', 8 + len(body)) + body)
    print('Wrote platform/icon/derp.ico, derp.png and derp.icns')


if __name__ == '__main__':
    main()
