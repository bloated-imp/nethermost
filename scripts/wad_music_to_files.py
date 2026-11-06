#!/usr/bin/env python3
"""Music WAD to music files: takes the songs (D_ lumps) out of a WAD whose
music is recorded audio, such as a PlayStation music WAD, and writes them as
files the game plays from its music folder (platform/user_music.c), named
after the song: D_E1M1.ogg, D_INTER.ogg ...

Ogg Vorbis, MP3, FLAC and WAV lumps are written as they are. Anything else
the game can't read (WMA, as in the 2007 PSX music WAD) is converted to Ogg
Vorbis with ffmpeg, which must be on the PATH, at quality 6: well above
what such low-bitrate sources hold, so little more is lost. MIDI and MUS
lumps are left out; the game plays those from the WAD itself.

    python3 scripts/wad_music_to_files.py psxdoom.wad "<save folder>/music"

The save folder is where doom.cfg is: on Windows
%APPDATA%/Local Games/DOOM. Existing files of the same name are replaced.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile


def kind(data):
    if data[:4] == b"OggS":
        return "ogg"
    if data[:4] == b"fLaC":
        return "flac"
    if data[:4] == b"RIFF" and data[8:12] == b"WAVE":
        return "wav"
    if data[:3] == b"ID3" or (len(data) > 1 and data[0] == 0xFF and data[1] & 0xE0 == 0xE0):
        return "mp3"
    if data[:4] in (b"MUS\x1a", b"MThd"):
        return "score"
    return None


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    wad, out = sys.argv[1], sys.argv[2]
    data = open(wad, "rb").read()
    magic, count, offset = struct.unpack("<4sII", data[:12])
    if magic not in (b"IWAD", b"PWAD"):
        sys.exit(f"{wad} is not a WAD")
    os.makedirs(out, exist_ok=True)
    ffmpeg = shutil.which("ffmpeg")
    written = 0
    for i in range(count):
        at, size, raw = struct.unpack("<II8s", data[offset + 16 * i:offset + 16 * i + 16])
        name = raw.rstrip(b"\0").decode("ascii", "replace").upper()
        if not name.startswith("D_") or not size:
            continue
        lump = data[at:at + size]
        found = kind(lump)
        if found == "score":
            continue
        if found:
            open(os.path.join(out, f"{name}.{found}"), "wb").write(lump)
        else:
            if not ffmpeg:
                sys.exit(f"{name} needs converting and ffmpeg is not on the PATH")
            with tempfile.TemporaryDirectory() as folder:
                source = os.path.join(folder, name + ".bin")
                open(source, "wb").write(lump)
                result = subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", source,
                                         "-map_metadata", "-1", "-c:a", "libvorbis", "-q:a", "6",
                                         os.path.join(out, name + ".ogg")])
                if result.returncode:
                    print(f"{name}: ffmpeg could not convert it", file=sys.stderr)
                    continue
        written += 1
    print(f"{written} songs written to {out}")


if __name__ == "__main__":
    main()
