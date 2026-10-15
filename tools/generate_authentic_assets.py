import struct
import os

def generate():
    f = open("tools/DOOM2.WAD", "rb")
    ident, numlumps, infotableofs = struct.unpack("<4sII", f.read(12))
    f.seek(infotableofs)
    directory = {}
    for _ in range(numlumps):
        pos, size, name = struct.unpack("<II8s", f.read(16))
        name = name.decode('latin1').rstrip('\x00').upper()
        directory[name] = (pos, size)

    # 1. Extract PLAYPAL palette 0 (256 colors -> 12-bit RGB)
    pos, size = directory['PLAYPAL']
    f.seek(pos)
    pal_data = f.read(768)
    palette_12bit = []
    for i in range(256):
        r, g, b = pal_data[i*3], pal_data[i*3+1], pal_data[i*3+2]
        c12 = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)
        palette_12bit.append(c12)

    with open("src/v4/doom_palette_authentic.h", "w") as pal_out:
        pal_out.write("// Authentic Id Software DOOM 2 256-Color Palette (PLAYPAL Lump 0)\n")
        pal_out.write("#ifndef DOOM_PALETTE_AUTHENTIC_H\n#define DOOM_PALETTE_AUTHENTIC_H\n\n#include <stdint.h>\n\n")
        pal_out.write("static const uint16_t doom2_playpal_12bit[256] = {\n")
        for i in range(0, 256, 8):
            row = ", ".join(f"0x{palette_12bit[j]:03X}" for j in range(i, i+8))
            pal_out.write(f"    {row},\n")
        pal_out.write("};\n\n#endif // DOOM_PALETTE_AUTHENTIC_H\n")

    # 2. Extract Authentic Patches
    def extract_patch(name):
        pos, size = directory[name]
        f.seek(pos)
        data = f.read(size)
        w, h, lx, ty = struct.unpack("<hhhh", data[:8])
        return w, h, lx, ty, data

    # Core curated authentic DOOM 2 lumps
    lumps_to_extract = [
        "SHT2A0", # SSG Idle (59x55)
        "SPOSA1", # Shotgun Guy Walk (36x55)
        "TROOA1", # Imp Walk (41x57)
        "SARGA1", # Pinky Demon Walk (40x56)
        "STFST00",# Doomguy Face (24x29)
        "STFEVL0",# Doomguy Evil Grin (24x30)
    ]

    patch_info = {}
    patch_bytes = {}
    for lump in lumps_to_extract:
        w, h, lx, ty, raw = extract_patch(lump)
        patch_info[lump] = (w, h, lx, ty)
        patch_bytes[lump] = raw

    # Extract STARTAN wall patch (SW19_1) cropped/scaled to 64x64
    w_w, h_w, lx_w, ty_w, raw_wall = extract_patch("SW19_1")
    # Decode 64x64 linear texture from SW19_1
    col_offsets = struct.unpack(f"<{w_w}I", raw_wall[8:8+w_w*4])
    wall_64x64 = [0 for _ in range(64 * 64)]
    for x in range(min(64, w_w)):
        ofs = col_offsets[x]
        while ofs < len(raw_wall):
            topdelta = raw_wall[ofs]
            if topdelta == 0xFF:
                break
            length = raw_wall[ofs+1]
            src = raw_wall[ofs+3 : ofs+3+length]
            for i in range(length):
                y = topdelta + i
                if y < 64:
                    wall_64x64[y * 64 + x] = src[i]
            ofs += 4 + length

    f.close()

    # Write C header
    with open("src/v4/doom_assets_authentic.h", "w") as h_out:
        h_out.write("#ifndef DOOM_ASSETS_AUTHENTIC_H\n#define DOOM_ASSETS_AUTHENTIC_H\n\n#include <stdint.h>\n#include \"doom_defs.h\"\n\n")
        h_out.write("typedef struct {\n    int16_t width;\n    int16_t height;\n    int16_t leftoffset;\n    int16_t topoffset;\n    const uint8_t *data;\n} doom_patch_t;\n\n")
        for lump in lumps_to_extract:
            h_out.write(f"extern const doom_patch_t patch_{lump.lower()};\n")
        h_out.write("extern const uint8_t tex_startan3[4096];\n\n")
        h_out.write("void setup_authentic_doom2_palette(void);\n")
        h_out.write("void draw_authentic_patch(volatile uint8_t *vram, const doom_patch_t *patch, int x, int y);\n")
        h_out.write("void draw_authentic_patch_scaled(volatile uint8_t *vram, const doom_patch_t *patch, int screenX, int screenY, int spr_w, int spr_h, fixed_t tz);\n\n")
        h_out.write("#endif // DOOM_ASSETS_AUTHENTIC_H\n")

    # Write C source
    with open("src/v4/doom_assets_authentic.c", "w") as c_out:
        c_out.write("#include \"doom_assets_authentic.h\"\n")
        c_out.write("#include \"doom_palette_authentic.h\"\n")
        c_out.write("#include \"r_segs.h\"\n\n")

        for lump in lumps_to_extract:
            w, h, lx, ty = patch_info[lump]
            raw = patch_bytes[lump]
            raw_var = f"raw_{lump.lower()}"
            c_out.write(f"static const uint8_t {raw_var}[{len(raw)}] = {{\n")
            for i in range(0, len(raw), 16):
                c_out.write("    " + ", ".join(f"0x{b:02X}" for b in raw[i:i+16]) + ",\n")
            c_out.write("};\n\n")
            c_out.write(f"const doom_patch_t patch_{lump.lower()} = {{\n")
            c_out.write(f"    .width = {w}, .height = {h}, .leftoffset = {lx}, .topoffset = {ty}, .data = {raw_var}\n")
            c_out.write("};\n\n")

        # 64x64 STARTAN3 wall texture
        c_out.write("const uint8_t tex_startan3[4096] = {\n")
        for i in range(0, 4096, 16):
            c_out.write("    " + ", ".join(f"0x{b:02X}" for b in wall_64x64[i:i+16]) + ",\n")
        c_out.write("};\n\n")

        # Palette setup
        c_out.write("void setup_authentic_doom2_palette(void) {\n")
        c_out.write("    volatile uint32_t *clut = (volatile uint32_t *)VGA_PALETTE_BASE;\n")
        c_out.write("    for (int i = 0; i < 256; i++) clut[i] = doom2_playpal_12bit[i];\n")
        c_out.write("}\n\n")

        # 1:1 patch draw
        c_out.write("void draw_authentic_patch(volatile uint8_t *vram, const doom_patch_t *patch, int px, int py) {\n")
        c_out.write("    const uint8_t *data = patch->data;\n")
        c_out.write("    int w = patch->width;\n")
        c_out.write("    const uint32_t *col_offsets = (const uint32_t *)(data + 8);\n")
        c_out.write("    for (int col = 0; col < w; col++) {\n")
        c_out.write("        int scr_x = px + col;\n")
        c_out.write("        if (scr_x < 0 || scr_x >= SCREEN_W) continue;\n")
        c_out.write("        uint32_t ofs = col_offsets[col];\n")
        c_out.write("        while (1) {\n")
        c_out.write("            uint8_t topdelta = data[ofs];\n")
        c_out.write("            if (topdelta == 0xFF) break;\n")
        c_out.write("            uint8_t length = data[ofs + 1];\n")
        c_out.write("            const uint8_t *src = data + ofs + 3;\n")
        c_out.write("            int dest_y = py + topdelta;\n")
        c_out.write("            for (int i = 0; i < length; i++, dest_y++) {\n")
        c_out.write("                if (dest_y >= 0 && dest_y < SCREEN_H) vram[dest_y * SCREEN_W + scr_x] = src[i];\n")
        c_out.write("            }\n")
        c_out.write("            ofs += 4 + length;\n")
        c_out.write("        }\n")
        c_out.write("    }\n")
        c_out.write("}\n\n")

        # Scaled billboarded patch draw
        c_out.write("void draw_authentic_patch_scaled(volatile uint8_t *vram, const doom_patch_t *patch, int screenX, int screenY, int spr_w, int spr_h, fixed_t tz) {\n")
        c_out.write("    const uint8_t *data = patch->data;\n")
        c_out.write("    int w = patch->width;\n")
        c_out.write("    int h = patch->height;\n")
        c_out.write("    const uint32_t *col_offsets = (const uint32_t *)(data + 8);\n")
        c_out.write("    int drawStartX = screenX - spr_w / 2;\n")
        c_out.write("    int drawEndX   = screenX + spr_w / 2;\n")
        c_out.write("    if (drawStartX < 0) drawStartX = 0;\n")
        c_out.write("    if (drawEndX >= SCREEN_W) drawEndX = SCREEN_W - 1;\n")
        c_out.write("    for (int sx = drawStartX; sx <= drawEndX; sx++) {\n")
        c_out.write("        if (tz >= wall_dist_buffer[sx]) continue;\n")
        c_out.write("        int patch_col = ((sx - (screenX - spr_w / 2)) * w) / spr_w;\n")
        c_out.write("        if (patch_col < 0) patch_col = 0;\n")
        c_out.write("        if (patch_col >= w) patch_col = w - 1;\n")
        c_out.write("        uint32_t ofs = col_offsets[patch_col];\n")
        c_out.write("        while (1) {\n")
        c_out.write("            uint8_t topdelta = data[ofs];\n")
        c_out.write("            if (topdelta == 0xFF) break;\n")
        c_out.write("            uint8_t length = data[ofs + 1];\n")
        c_out.write("            const uint8_t *src = data + ofs + 3;\n")
        c_out.write("            for (int i = 0; i < length; i++) {\n")
        c_out.write("                int patch_y = topdelta + i;\n")
        c_out.write("                int dest_y = screenY + (patch_y * spr_h) / h;\n")
        c_out.write("                if (dest_y >= 0 && dest_y < 168) vram[dest_y * SCREEN_W + sx] = src[i];\n")
        c_out.write("            }\n")
        c_out.write("            ofs += 4 + length;\n")
        c_out.write("        }\n")
        c_out.write("    }\n")
        c_out.write("}\n")

    print("[GEN] Success!")

if __name__ == "__main__":
    generate()
