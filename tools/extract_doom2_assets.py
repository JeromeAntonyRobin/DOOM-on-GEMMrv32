import struct
import os

class WadReader:
    def __init__(self, wad_path):
        self.f = open(wad_path, "rb")
        ident, numlumps, infotableofs = struct.unpack("<4sII", self.f.read(12))
        self.ident = ident.decode('latin1')
        self.numlumps = numlumps
        self.infotableofs = infotableofs
        
        self.f.seek(infotableofs)
        self.directory = []
        for _ in range(numlumps):
            pos, size, name = struct.unpack("<II8s", self.f.read(16))
            name = name.decode('latin1').rstrip('\x00').upper()
            self.directory.append({'pos': pos, 'size': size, 'name': name})
            
        self.name_map = {}
        for idx, entry in enumerate(self.directory):
            self.name_map[entry['name']] = idx

    def get_lump(self, name):
        if name not in self.name_map:
            return None
        idx = self.name_map[name]
        entry = self.directory[idx]
        self.f.seek(entry['pos'])
        return self.f.read(entry['size'])

    def decode_patch(self, data):
        """Decode a Doom picture format lump into (width, height, left_ofs, top_ofs, pixels_2d)"""
        width, height, left_ofs, top_ofs = struct.unpack("<hhHH", data[0:8])
        col_offsets = struct.unpack(f"<{width}I", data[8:8+width*4])
        # 2D array of pixels [y][x], 0xFF represents transparent
        pixels = [[0xFF for _ in range(width)] for _ in range(height)]
        for x in range(width):
            ofs = col_offsets[x]
            while ofs < len(data):
                topdelta = data[ofs]
                if topdelta == 0xFF:
                    break
                length = data[ofs + 1]
                # data[ofs+2] is unused padding
                post_pixels = data[ofs + 3 : ofs + 3 + length]
                for y_idx in range(length):
                    py = topdelta + y_idx
                    if py < height:
                        pixels[py][x] = post_pixels[y_idx]
                ofs += 4 + length
        return width, height, left_ofs, top_ofs, pixels

def main():
    wad = WadReader("tools/DOOM2.WAD")
    print(f"WAD Type: {wad.ident}, Total Lumps: {wad.numlumps}")
    
    # Check PLAYPAL
    playpal = wad.get_lump("PLAYPAL")
    if playpal:
        print(f"PLAYPAL found! Size: {len(playpal)} bytes (Palettes: {len(playpal)//768})")
    
    # Check key sprites
    test_sprites = [
        "SHT2A0", "SHT2B0", "SHT2C0", "SHT2D0", # Super shotgun
        "SPOSA1", "TROOA1", "SARGA1", "HEADB1", # Monsters
        "STBAR", "STFST00", "STFEVL0"           # Status bar & Face
    ]
    for sp in test_sprites:
        data = wad.get_lump(sp)
        if data:
            w, h, lx, ty, _ = wad.decode_patch(data)
            print(f"Found {sp}: {w}x{h}, offset=({lx},{ty}), raw lump size={len(data)} bytes")
        else:
            print(f"Missing {sp}")

if __name__ == "__main__":
    main()
