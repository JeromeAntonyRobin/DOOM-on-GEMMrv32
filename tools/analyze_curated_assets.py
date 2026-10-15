import struct

def analyze_wad():
    f = open("tools/DOOM2.WAD", "rb")
    ident, numlumps, infotableofs = struct.unpack("<4sII", f.read(12))
    f.seek(infotableofs)
    directory = []
    for _ in range(numlumps):
        pos, size, name = struct.unpack("<II8s", f.read(16))
        name = name.decode('latin1').rstrip('\x00').upper()
        directory.append({'pos': pos, 'size': size, 'name': name})
    f.close()

    name_map = {entry['name']: entry for entry in directory}

    # Group categories
    categories = {
        "Status Bar & Faces": ["STBAR", "STFST00", "STFST01", "STFEVL0", "STFKILL0", "STFDEAD0", "STTNUM0", "STTNUM1", "STTNUM2", "STTNUM3", "STTNUM4", "STTNUM5", "STTNUM6", "STTNUM7", "STTNUM8", "STTNUM9"],
        "Super Shotgun (SHT2)": [f"SHT2{chr(c)}0" for c in range(ord('A'), ord('J')+1)],
        "Shotgun Guy (SPOS)": [entry['name'] for entry in directory if entry['name'].startswith('SPOS')],
        "Imp (TROO)": [entry['name'] for entry in directory if entry['name'].startswith('TROO')],
        "Pinky Demon (SARG)": [entry['name'] for entry in directory if entry['name'].startswith('SARG')],
        "Cacodemon (HEAD)": [entry['name'] for entry in directory if entry['name'].startswith('HEAD')],
        "Barrels & Projectiles": ["BAR1A0", "BAR1B0", "BEXP0", "BEXPA0", "BEXPB0", "BEXPC0", "BAL1A0", "BAL1B0"],
        "Common Flats (64x64)": ["FLOOR4_8", "CEIL3_5", "FLOOR0_1", "FLAT1", "NUKAGE3", "SLIME01", "GRASS1", "RROCK01"],
    }

    print("=== AUTHENTIC DOOM 2 ASSET SPACE BREAKDOWN ===")
    total_curated = 0
    for cat, names in categories.items():
        cat_size = 0
        found = 0
        for name in names:
            if name in name_map:
                cat_size += name_map[name]['size']
                found += 1
        total_curated += cat_size
        print(f"{cat:<28} : {found:>3} lumps, {cat_size:>8} bytes ({cat_size/1024:>6.1f} KB)")

    print("-" * 50)
    print(f"Total Curated Assets Size: {total_curated} bytes ({total_curated/1024:.1f} KB)")

if __name__ == "__main__":
    analyze_wad()
