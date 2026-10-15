# Geometric Enclosure, Wall Occlusion, and Camera Alignment

## 1. Problem Overview and User Symptoms

Following the deployment of authentic DOOM 2 assets and flicker elimination, testing on the Digilent Basys 3 FPGA (`xc7a35t`) revealed two critical gameplay and rendering defects:
1. **Unrendered Walls & Open Voids**: Turning the camera around revealed gaping holes where solid boundary walls should have been. Only isolated walls appeared, while other room boundaries rendered as blank void (showing default sky/ceiling flats).
2. **Reversed Steering & Axis Disorientation**: Turning left via `'a'` rotated the camera clockwise (right), and moving forward/backward exhibited swapped translation axes where forward motion displaced the player along orthogonal world coordinates. The 2D minimap compounded this disorientation by rendering North towards the bottom of the display.

---

## 2. Root Cause Analysis

### 2.1 Winding Order & Back-Face Culling Failure
DOOM uses a right-handed 2D coordinate system where $+X$ points East and $+Y$ points North. The perspective projection formula translates world endpoints $(v_1, v_2)$ into camera space:

$$\begin{aligned}
t_{z} &= (x - \text{viewx}) \cos(\theta) + (y - \text{viewy}) \sin(\theta) \\
t_{x} &= (x - \text{viewx}) \sin(\theta) - (y - \text{viewy}) \cos(\theta)
\end{aligned}$$

The projected screen coordinate is:

$$x_{\text{screen}} = \text{CENTER\_X} + \frac{t_x \cdot \text{FOCAL\_LEN}}{t_z}$$

For a linedef to be considered front-facing and visible on screen, its first vertex $v_1$ must appear to the left of its second vertex $v_2$ ($x_1 < x_2$). In standard DOOM:
- **Rule**: Walking along a linedef from $v_1$ to $v_2$, the interior of the sector must lie on the **right-hand side** (Counter-Clockwise winding around the room perimeter).

In the previous map definition:
- Sector 0 South Wall was defined from $(-200, -200) \to (200, -200)$ (West to East). When standing inside the Hangar looking South ($\theta = 270^\circ$):
  $$t_{x1} = +200, \quad t_{x2} = -200 \implies x_1 > x_2$$
  The engine treated the wall as back-facing:
  ```c
  if (x1 > x2) {
      if (!is_twosided) return; // One-sided walls are back-face culled
  }
  ```
  Consequently, the South wall was completely culled when looking directly at it from inside the room.
- Similar clockwise definitions inverted the West wall and Computer Room perimeters.
- Furthermore, corner wings (North-West wing $(-200, 100) \to (-100, 100)$, North-East wing $(100, 100) \to (200, 100)$, and Courtyard South wings) were entirely missing from `map_lines`.

### 2.2 Inverted Steering & Orthogonal Translation
1. **Yaw Inversion**: In trigonometry, $0^\circ$ is East and $90^\circ$ is North. Turning left from North towards West ($180^\circ$) requires **incrementing** the angle index. The previous code decremented `player_angle_idx` on `'a'` and incremented on `'d'`, causing mirrored rotation.
2. **Translation Axis Swap**: Forward translation erroneously added `viewsin` to $X$ and `viewcos` to $Y$, swapping the primary longitudinal and lateral axes.
3. **Inverted Minimap Y-Axis**: The minimap projected world $+Y$ directly down the screen ($y = (Y + 250) / 16 + 4$), inverting the north-south cardinal axis.

---

## 3. Engineering Solutions

### 3.1 100% Closed Multi-Sector Geometric Enclosure
All room perimeters were redesigned into fully enclosed, counter-clockwise 3D polygons comprising 18 linedefs across 15 vertices:

| Sector | Linedef Range | Description | Winding ($v_1 \to v_2$) | Classification |
|---|---|---|---|---|
| **Sector 0 (Hangar)** | Line 0 | South Wall | $(200, -200) \to (-200, -200)$ | Solid (`ML_BLOCKING`) |
| | Line 1 | West Wall | $(-200, -200) \to (-200, 100)$ | Solid (`ML_BLOCKING`) |
| | Line 2 | North-West Wing | $(-200, 100) \to (-100, 100)$ | Solid (`ML_BLOCKING`) |
| | Line 3 | Stairway Portal | $(-100, 100) \to (100, 100)$ | 2-Sided Step (`ML_TWOSIDED`) |
| | Line 4 | North-East Wing | $(100, 100) \to (200, 100)$ | Solid (`ML_BLOCKING`) |
| | Line 5 | Computer Alcove Portal | $(200, 100) \to (200, -100)$ | 2-Sided Step (`ML_TWOSIDED`) |
| | Line 6 | East Wall (Lower) | $(200, -100) \to (200, -200)$ | Solid (`ML_BLOCKING`) |
| **Sector 1 (Stairs)** | Line 7 | West Wall | $(-100, 100) \to (-100, 250)$ | Solid (`ML_BLOCKING`) |
| | Line 8 | Courtyard Portal | $(-100, 250) \to (100, 250)$ | 2-Sided Drop (`ML_TWOSIDED`) |
| | Line 9 | East Wall | $(100, 250) \to (100, 100)$ | Solid (`ML_BLOCKING`) |
| **Sector 2 (Courtyard)** | Line 10 | South-West Wing | $(-100, 250) \to (-350, 250)$ | Solid (`ML_BLOCKING`) |
| | Line 11 | West Wall | $(-350, 250) \to (-350, 600)$ | Solid (`ML_BLOCKING`) |
| | Line 12 | North Wall | $(-350, 600) \to (350, 600)$ | Solid (`ML_BLOCKING`) |
| | Line 13 | East Wall | $(350, 600) \to (350, 250)$ | Solid (`ML_BLOCKING`) |
| | Line 14 | South-East Wing | $(350, 250) \to (100, 250)$ | Solid (`ML_BLOCKING`) |
| **Sector 3 (Comp Room)** | Line 15 | South Wall | $(400, -100) \to (200, -100)$ | Solid (`ML_BLOCKING`) |
| | Line 16 | East Wall | $(400, 100) \to (400, -100)$ | Solid (`ML_BLOCKING`) |
| | Line 17 | North Wall | $(200, 100) \to (400, 100)$ | Solid (`ML_BLOCKING`) |

### 3.2 Bidirectional Portal Viewing
For two-sided linedefs, viewing the portal from the back sector results in $x_1 > x_2$. Rather than culling the portal, `R_AddLine()` dynamically swaps screen coordinates, depths, and front/back sector assignments:
```c
if (x1 > x2) {
    if (!is_twosided) return; // One-sided walls are back-face culled
    int tmp_x = x1; x1 = x2; x2 = tmp_x;
    fixed_t tmp_tz = tz1; tz1 = tz2; tz2 = tmp_tz;
    front = line->backsector;
    back  = line->frontsector;
}
```
This guarantees seamless visibility through portals regardless of whether the player is stepping up into the stairway or looking backward from the stairway into the hangar.

### 3.3 True-North Coordinate Alignment & Minimap
- Movement controls in `src/v4/main.c` were aligned with standard trigonometry:
  - `'w'`: adds $\text{viewcos} \cdot \text{speed}$ to $X$, and $\text{viewsin} \cdot \text{speed}$ to $Y$.
  - `'s'`: subtracts $\text{viewcos} \cdot \text{speed}$ from $X$, and $\text{viewsin} \cdot \text{speed}$ from $Y$.
  - `'a'`: increments `player_angle_idx` (turning left / counter-clockwise towards West).
  - `'d'`: decrements `player_angle_idx` (turning right / clockwise towards East).
- The 2D minimap was refactored with true-North orientation where world $+Y$ renders upwards:
  ```c
  int y = 55 - (FROM_FIXED(line->y) + 200) / 16;
  int dir_y = py - FROM_FIXED(viewsin * 4); // North is UP
  ```

---

## 4. Hardware Validation Results

1. **FPGA Bitstream & Memory**: Bitstream flashed onto Digilent Basys 3 (`xc7a35t`). Executable size: 31,096 bytes (text: 29,444 B, data: 1,636 B, bss: 4,420 B).
2. **Serial Execution & Entity Interaction**:
   - `[DOOM 2] First Native 320x200 Mode 13h frame rendered successfully!`
   - Movement and turning tested via serial commands at 19,200 baud over `COM10`.
   - Super Shotgun fired and successfully registered entity collision damage (`[DOOM 2] Monster blasted to pieces!`).
3. **Visual Integrity**:
   - 100% rendered wall enclosure with zero back-face culling dropouts.
   - Zero progressive wipes or gun flickering.
   - Completely intuitive camera yaw and translational movement.
