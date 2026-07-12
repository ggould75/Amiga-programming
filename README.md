# Amiga-programming

Personal collection of Amiga C programs, written with **SAS/C 6.0** and targeting
**Kickstart 2.05 (V37)**, built and run on a real Amiga (when I'm home), and in FS-UAE when I'm not.

## Programs

Reference catalogue of what each program under `C_Programs/` does, so a specific
feature is easy to find and reuse later. The `Intuition/` set is a deliberate
progression, each program building on the previous one.

### Root

- **`hello.c`** — Minimal SAS/C sanity check: `printf` plus a `dos.library`
  `Delay()`. Smoke-test that the compile/link/run chain works.
  *Libraries: dos.*

### `Intuition/`

- **`open_window.c`** — Simplest possible window: fill a `NewWindow`,
  `OpenWindow()`, spin a busy-wait loop, `CloseWindow()`. No IDCMP, no gadgets.
  *Libraries: intuition (rev 0).*

- **`open_window_gadget.c`** — Adds the system gadgets (close / drag / depth /
  sizing) and `NOCAREREFRESH` so Intuition auto-repaints; blocks on the window's
  IDCMP signal until the close gadget is hit.
  *Libraries: intuition (rev 0).*

- **`open_window_in_screen.c`** — Opens a custom `CUSTOMSCREEN` (320×200, depth 2)
  with its own Topaz `TextAttr` font, then a window on that screen and draws
  "Hello World" via graphics `Text()`.
  *Libraries: intuition, graphics (rev 0).*

- **`detect_mouse_pos.c`** — Tracks the pointer with `REPORTMOUSE` +
  `IDCMP_MOUSEMOVE`, printing live coordinates; manual `BeginRefresh`/`EndRefresh`
  on a `SMART_REFRESH` window. Old-style `NewWindow` API. Accepts `gzz` /
  `borderless` CLI flags (GimmeZeroZero / Borderless).
  *Libraries: intuition (rev 37), graphics.*

- **`detect_mouse_pos_v37.c`** — Same mouse tracking, rewritten with the V37 tag
  API (`OpenWindowTagList` + `TagItem` array), `GetScreenDrawInfo()` for the
  screen's pens/font, and tidy `handleIDCMP()` / `cleanExit()` structure. Same
  `gzz` / `borderless` flags.
  *Libraries: intuition, graphics (rev 37).*

- **`window_redraw.c`** — Focuses on correct refresh handling: acknowledge the
  refresh inside a bare `BeginRefresh`/`EndRefresh` pair (lets Intuition repaint
  the frame + gadgets), then redraw your own content *outside* that pair via
  `drawStaticContent()`. Same `gzz` / `borderless` flags.
  *Libraries: intuition, graphics (rev 37).*

- **`window_redraw_regions.c`** — Adds a reusable interior clip `Region` installed
  with `InstallClipRegion()` so long text can't paint over the border gadgets;
  demonstrates the correct region lifecycle (build off-layer with
  `NewRegion`/`OrRectRegion`/`ClearRegion`, install only around your own drawing,
  remove before the window closes).
  *Libraries: intuition, graphics, layers (rev 37).*

### `Boing/`

- **`boing5.c`** — Jimmy Maher's reconstruction of the famous Amiga Boing demo
  (<http://amiga.filfre.net>). I only fixed compiler warnings and minor issues to
  build under 2.05 / SAS/C 6.0. See `Boing/README.md`.
  *Libraries: intuition, graphics, dos, audio.device (rev 37).*
