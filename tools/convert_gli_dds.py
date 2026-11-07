"""Convert RTX Remix capture textures written with the GLI library ("GLI1" FourCC) into standard DDS.

Remix writes Timeline's 16-bit textures as DDS with FourCC "GLI1" plus a 20-byte GLI extension header,
which NVIDIA Texture Tools, Photoshop and most DDS readers reject. The pixel data is plain D3D9 R5G6B5
(red in the high bits; the masks GLI writes into the header are R/B swapped). Files already in a standard
layout (e.g. DX10 RGBA8) are copied unchanged so the output folder is complete.

Usage: python -I convert_gli_dds.py <capture textures dir> <output dir>
"""
import shutil
import struct
import sys
from pathlib import Path

HEADER_SIZE = 128                  # "DDS " + DDS_HEADER
GLI_EXTENSION_SIZE = 20
PIXEL_FORMAT_OFFSET = 76           # DDS_PIXELFORMAT inside the file
FLAGS_OFFSET, PITCH_OFFSET, WIDTH_OFFSET = 8, 20, 16
DDSD_PITCH, DDSD_LINEARSIZE = 0x8, 0x80000
DDPF_RGB = 0x40
R5G6B5_MASKS = (0xF800, 0x07E0, 0x001F, 0x0000)


def is_gli(data: bytes) -> bool:
    return data[:4] == b"DDS " and data[84:88] == b"GLI1"


def to_standard_r5g6b5(data: bytes) -> bytes:
    """Rewrites a GLI1 16-bit DDS as a standard RGB 5:6:5 DDS; raises ValueError for other GLI formats."""
    bits = struct.unpack_from("<I", data, 88)[0]
    if bits != 16:
        raise ValueError(f"unsupported GLI1 format: {bits} bits per pixel")

    header = bytearray(data[:HEADER_SIZE])
    width = struct.unpack_from("<I", header, WIDTH_OFFSET)[0]
    flags = struct.unpack_from("<I", header, FLAGS_OFFSET)[0]
    struct.pack_into("<I", header, FLAGS_OFFSET, (flags | DDSD_PITCH) & ~DDSD_LINEARSIZE)
    struct.pack_into("<I", header, PITCH_OFFSET, width * 2)
    struct.pack_into("<8I", header, PIXEL_FORMAT_OFFSET, 32, DDPF_RGB, 0, 16, *R5G6B5_MASKS)
    return bytes(header) + data[HEADER_SIZE + GLI_EXTENSION_SIZE:]


def main() -> None:
    source, target = Path(sys.argv[1]), Path(sys.argv[2])
    target.mkdir(parents=True, exist_ok=True)
    converted = copied = 0
    for path in sorted(source.glob("*.dds")):
        data = path.read_bytes()
        if is_gli(data):
            (target / path.name).write_bytes(to_standard_r5g6b5(data))
            converted += 1
        else:
            shutil.copy2(path, target / path.name)
            copied += 1
    print(f"converted {converted} GLI1 textures, copied {copied} standard ones -> {target}")


if __name__ == "__main__":
    main()
