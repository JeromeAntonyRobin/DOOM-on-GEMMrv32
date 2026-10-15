import struct
import sys

def main():
    import argparse
    parser = argparse.ArgumentParser(description="NEORV32 executable image generator")
    parser.add_argument("-i", "--input", required=True, help="Input raw binary file")
    parser.add_argument("-o", "--output", required=True, help="Output exe binary file")
    parser.add_argument("-b", "--base", default="0", help="Base address in hex (default 0)")
    parser.add_argument("-t", "--type", default="exe", help="Output type (default exe)")
    args = parser.parse_args()

    base_addr = int(args.base, 0)
    with open(args.input, "rb") as f:
        data = bytearray(f.read())

    # Pad to 4-byte boundary
    pad = (4 - (len(data) % 4)) % 4
    data.extend(b"\x00" * pad)
    raw_size = len(data)

    checksum = 0
    for i in range(0, raw_size, 4):
        word = struct.unpack("<I", data[i:i+4])[0]
        checksum = (checksum + word) & 0xFFFFFFFF

    checksum = (~checksum) & 0xFFFFFFFF
    sig = 0x214F454E # 'NEO!'

    header = struct.pack("<IIII", sig, base_addr, raw_size, checksum)
    with open(args.output, "wb") as f:
        f.write(header)
        f.write(data)

    print(f"Generated {args.output}: {raw_size} bytes @ 0x{base_addr:08X}, checksum = 0x{checksum:08X}")

if __name__ == "__main__":
    main()
