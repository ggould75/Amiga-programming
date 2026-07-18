# Amiga ROM Kernel Reference Manual v2.04 — Examples

Worked-through examples from the **Amiga ROM Kernel Reference Manual: Libraries**
(2.04 / V37 edition), kept alongside my own annotations. Each program is the
manual's listing typed in, built with **SAS/C 6.0** for **Kickstart 2.05 (V37)**,
with extra comments where I wanted to understand *why* the code is shaped the way
it is. Files are organised by the manual's chapters.

## Programs

Reference catalogue of what each example demonstrates, so a specific technique is
easy to find later.

### `Chapter 6 - Intuition menus/`

- **`simple_menu.c`** — The manual's basic menu example: build a `Menu` /
  `MenuItem` / `IntuiText` strip by hand (a single "Project" menu with
  Open/Save/Print/Quit, a `Print` sub-menu of Draft/NLQ), attach it to a window
  with `SetMenuStrip()`, and decode `IDCMP_MENUPICK` events via
  `ItemAddress()` + the `MENUNUM`/`ITEMNUM`/`SUBNUM` macros, following the
  `NextSelect` chain for drag-selections. Menu widths are laid out from the
  Topaz-8 font; header width comes from `TextLength()`. My comments annotate the
  `MenuItem` fields and the font-height dependence of `MENHEIGHT`.
  *Libraries: intuition, graphics (rev 37).*

### `Chapter 30 - Layers Library/`

- **`layers.c`** — The manual's tour of the layers library, driven on a
  hand-built low-level `View`/`ViewPort` (not an Intuition screen, as the manual
  requires) at 320×200, depth 2. Creates three overlapping layers — one
  **super-bitmap** (`LAYERSUPER`), one **smart** (`LAYERSMART`), one **simple**
  (`LAYERSIMPLE`) — with `CreateBehindLayer()`/`CreateUpfrontLayer()`, then
  exercises `MoveLayer`, `MoveLayerInFrontOf`, `UpfrontLayer`, `BehindLayer`,
  `SizeLayer`, and `ScrollLayer`, relabelling each layer after moves to show which
  refresh types repaint themselves. Manages its own `BitMap`/bitplane allocation
  (`AllocRaster`/`FreeRaster`) and restores the prior view on exit. My comments
  cover the SAS/C auto-open-library trick (declare-but-don't-define the bases,
  `__OSlibversion = 37`) used instead of manual `OpenLibrary()`.
  *Libraries: graphics, layers, exec, dos (rev 37).*
