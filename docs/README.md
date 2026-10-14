## Documentation

[![GitHub Pages](https://img.shields.io/website.svg?label=stnolting.github.io%2Fneorv32&longCache=true&style=flat-square&url=http%3A%2F%2Fstnolting.github.io%2Fneorv32%2Findex.html&logo=GitHub)](https://stnolting.github.io/neorv32)
[![Documentation](https://img.shields.io/github/actions/workflow/status/stnolting/neorv32/Documentation.yml?branch=main&longCache=true&style=flat-square&label=Documentation&logo=Github%20Actions&logoColor=fff)](https://github.com/stnolting/neorv32/actions/workflows/Documentation.yml)

> [!TIP]
> The ratified RISC-V specs. are available online: https://docs.riscv.org/reference/home/

---

### > [`GEMMRV_DOOM_GPU.md`](GEMMRV_DOOM_GPU.md)
Comprehensive technical specification for the **GEMMrv32-DOOM!** standalone graphics architecture:
- 160x120 @ 8bpp VGA video controller (upscaled 4x to 640x480 @ 60Hz).
- 256-color palette (CLUT) in distributed LUT RAM.
- PS/2 keyboard controller with digital deglitching and on-chip FIFO.
- Complete C driver API reference (`gemmrv_gpu.h` / `gemmrv_gpu.c`) for display primitives, texture column blitting, and systolic matrix transformations.

---

### > [`datasheet`](datasheet)

AsciiDoc sources for the NEORV32 data sheet. The online version of the data sheet is
available at [https://stnolting.github.io/neorv32](https://stnolting.github.io/neorv32).

Build the HTML data sheet with `neorv32/docs$ make html` and open `neorv32/docs/public/index.html`.

### > [`figures`](figures)

Images used by the data sheet, user guide and the GitHub pages. The according
licenses are listed in `figures/license.md`.

### > [`userguide`](userguide)

AsciiDoc sources for the NEORV32 user guide. The online version of the user guide is
available at [https://stnolting.github.io/neorv32/ug](https://stnolting.github.io/neorv32/ug).

Build the HTML user guide with `neorv32/docs$ make ug-html` and open `neorv32/docs/public/ug/index.html`.
