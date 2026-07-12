# Boing implementation notes — bitmaps, screens and animation

General notes on the Amiga display model, written while taking apart
`C_Programs/Boing/boing5.c` (Jimmy Maher's reconstruction of the 1984 Luck/Mical demo).
The *techniques* are the point; Boing is just the worked example they're drawn from.

Target throughout: **A500 Plus, OS 2.05 (V37), ECS, PAL, plain 68000, SAS/C 6.0.**

What's in here:

- **The four structures** — `BitMap`, `RastPort`, `ViewPort`, `Screen`: what each one
  actually is, and how they hang together.
- **Bitplanes** — planar pixel encoding, allocating and freeing plane memory, chip RAM
  budgeting, and how many planes ECS can really display.
- **Drawing into one specific plane** — write masks, `PlanePick`/`PlaneOnOff`, and using
  a high plane as a palette selector to get free compositing.
- **Animation, cheapest first** — scrolling the ViewPort (no drawing at all), palette
  cycling, double buffering, blitter/Bobs, hardware sprites, and when to reach for each.
- **Opening a custom screen** — `NewScreen` vs `OpenScreenTags`, `CUSTOMBITMAP` ownership
  rules, and the teardown order that keeps the copper out of freed memory.
- **Recurring discipline** — the mistakes that keep coming back.
- **What's 3.x-only** — so it doesn't get reached for by accident.

---

## 1. The four structures, and how they relate

This is the mental model everything else hangs off. Four different things are easy to
confuse because they all sound like "the screen":

| Structure | What it actually is | Header |
|---|---|---|
| `BitMap` | **The memory.** Where the pixels live. | `<graphics/gfx.h>` |
| `RastPort` | **The drawing context.** How you *write into* a BitMap. | `<graphics/rastport.h>` |
| `ViewPort` | **The display context.** How a BitMap is *shown* by the hardware. | `<graphics/view.h>` |
| `Screen` | **Intuition's bundle** of all three, plus windows, title bar, etc. | `<intuition/screens.h>` |

The ownership chain the hardware actually walks:

```
View  (one per machine — Intuition owns it)
 └─> ViewPort   (one per Screen, chained via ViewPort.Next)
      ├─> RasInfo      -> BitMap  -> Planes[0..7] -> chip RAM
      │     RxOffset/RyOffset = which pixel of the BitMap lands at top-left
      └─> ColorMap     -> ColorTable (the 32 hardware colour registers)
```

Key points:

- **`RastPort` and `ViewPort` are independent views of the same `BitMap`.** The
  RastPort says "draw a line, in pen 3, JAM1, into *this* BitMap". The ViewPort says
  "put *this* BitMap on the screen, starting at this offset, in this display mode,
  with these colours". They never talk to each other. This is why you can draw into a
  bitmap that isn't being displayed (double buffering), and why you can change what's
  displayed without drawing anything at all (**that's the whole Boing trick**).
- **A `Screen` embeds them by value, not by pointer:**
  ```c
  struct Screen {
      ...
      struct ViewPort ViewPort;   /* embedded */
      struct RastPort RastPort;   /* embedded */
      struct BitMap   BitMap;     /* embedded — and it's a COPY */
      ...
  };
  ```
  **Consequence that bites people:** with `CUSTOMBITMAP`, Intuition *copies* your
  `struct BitMap` into `Screen->BitMap`. After `OpenScreen()`, mutating your original
  global `BitMap` does nothing. You must poke `Screen->BitMap.Planes[n]`, which is what
  `boing5.c` does. (The `Planes[]` *pointers* are copied too — the plane memory itself
  is shared, so you still own it and still must free it.)
- **A `Window` has its own `RastPort`** (`Window->RPort`), whose `Layer` clips drawing
  to the visible part of the window, and whose BitMap is the screen's BitMap. Drawing
  into `Screen->RastPort` instead goes **straight to the bitmap with no layer and no
  clipping** — fast, but it will happily scribble over other windows. Boing does this
  deliberately: it owns the whole screen.

---

## 2. Bitplanes and the `BitMap`

### Planar, not chunky

The Amiga does **not** store one byte per pixel. A pixel's colour number is assembled
bit-by-bit from the *same coordinate* in each bitplane:

```
plane 0 (LSB):  ...0...      pixel colour index = 0b01101 = 13
plane 1:        ...1...                            ^^^^^
plane 2:        ...1...                            | | | plane 0
plane 3:        ...0...                            | | plane 1
plane 4 (MSB):  ...1...                            ...
```

So *depth N gives 2^N colours*, and each plane is an independent, contiguous
one-bit-per-pixel raster.

### `struct BitMap`

```c
struct BitMap {
    UWORD   BytesPerRow;    /* width in bytes — always EVEN (word-aligned) */
    UWORD   Rows;           /* height in scanlines */
    UBYTE   Flags;
    UBYTE   Depth;          /* number of planes actually in use */
    UWORD   pad;
    PLANEPTR Planes[8];     /* <-- eight. This is the structural ceiling. */
};
```

- `InitBitMap(&bm, depth, width, height)` fills in `BytesPerRow`, `Rows`, `Depth`.
  **It does not allocate anything.** You still have to fill `Planes[]`.
- `BytesPerRow = width / 8`, rounded up to an even number. A 336-pixel-wide bitmap is
  **42 bytes per row**. A 320-wide one is 40.
- Plane size = `BytesPerRow * Rows` = `RASSIZE(width, height)` (macro in
  `<graphics/gfx.h>`).

### Allocating the planes

Two idioms:

```c
/* (a) The normal way — one AllocRaster per plane. */
InitBitMap(&bm, DEPTH, WIDTH, HEIGHT);
for (i = 0; i < DEPTH; i++) {
    bm.Planes[i] = (PLANEPTR)AllocRaster(WIDTH, HEIGHT);
    if (!bm.Planes[i]) cleanup();
    BltClear(bm.Planes[i], RASSIZE(WIDTH, HEIGHT), 1);
}
...
for (i = 0; i < DEPTH; i++)
    if (bm.Planes[i]) FreeRaster(bm.Planes[i], WIDTH, HEIGHT);
```

`AllocRaster()` gives you **chip RAM** (Denise must DMA it), word-aligned. Pair it with
`FreeRaster()` and pass back the *same width and height* — it can't remember them.

```c
/* (b) One big chunk, planes carved out of it — when you need them CONTIGUOUS. */
PlanePtr = AllocMem(4536 + 4*9072, MEMF_CHIP);
for (i = 0; i < 4; i++)
    bm.Planes[i] = PlanePtr + 4536 + (i * 9072);
```

Use (b) when you intend to do pointer arithmetic across planes, or need slack *before*
plane 0 so a negative `RyOffset` reads real memory instead of random chip RAM. Boing's
4536 bytes = 108 rows × 42 bytes of lead-in. Free the **one** allocation, not the
individual planes.

### Chip RAM is the real constraint

Bitplanes, sprite data, `struct Image` data, audio samples, copper lists, and blitter
temporaries must all be `MEMF_CHIP`. The A500 Plus has **1 MB chip** (plus 1 MB of
trapdoor RAM which is *also* chip on a Plus, unless jumpered otherwise — but budget as
if it's tight). A 5-plane 336×216 bitmap is ~45 KB; Boing's 16 pre-shifted background
planes cost **145 KB**. Do the arithmetic *before* you allocate:

```
bytes = ((width + 15) / 16 * 2) * height * depth
```

Always `MEMF_CLEAR` unless you're about to overwrite every byte — uninitialised chip RAM
shows up on screen as garbage, which is a confusing way to learn you forgot.

### How many bitplanes can I actually have?

The `BitMap` struct holds **8**, but ECS cannot *display* 8:

| Mode (ECS, PAL) | Max planes | Colours |
|---|---|---|
| Lores (320) | 5 | 32 |
| Lores EHB (Extra Half-Brite) | 6 | 64 (upper 32 = lower 32 at half brightness) |
| Lores HAM6 | 6 | 4096 (hold-and-modify) |
| Hires (640) | **4** | 16 |
| SuperHires (1280, ECS Denise) | 2 | 4 |
| Dual playfield (lores) | 3 + 3 | 8 + 8 |

**8 planes / 256 colours is AGA only.** So is a 256-colour hires screen. If a design
needs more than 16 colours at 640 wide, it does not run on this machine — that's a real
constraint, not a fixable one.

---

## 3. Drawing into a *specific* bitplane

Four techniques, roughly in order of how often you'll want them:

### (a) The write mask — `RastPort.Mask`

Every graphics primitive (`Move`/`Draw`, `RectFill`, `Text`, `BltBitMapRastPort`)
honours the RastPort's write mask. A 0 bit disables that plane:

```c
#include <graphics/gfxmacros.h>

SetWrMsk(rp, 0x10);      /* enable ONLY plane 4 (bit 4) */
SetAPen(rp, 16);         /* any pen with bit 4 set will do */
Move(rp, 0, 0); Draw(rp, 100, 100);
SetWrMsk(rp, 0xFF);      /* ALWAYS put it back */
```

(`SetWrMsk` is just `rp->Mask = m`, but use the macro — it documents intent.)
Boing uses exactly this to draw the grid wall into plane 4 without touching the ball's
planes 0–3. **Caveats:** `BltMaskBitMapRastPort()` *ignores* `Mask`; `BltBitMapRastPort()`
respects it. Restore the mask when done — a stale mask produces beautifully baffling bugs.

### (b) `PlanePick` / `PlaneOnOff` on `struct Image`

For `DrawImage()`, an Image can have fewer planes than the destination:

```c
struct Image Ball = {
    0, 0, 144, 100, 4,      /* Left, Top, Width, Height, Depth */
    ball_bitplanes,
    15, 0,                  /* PlanePick = 0x0F, PlaneOnOff = 0x00 */
    NULL
};
```

- **`PlanePick`**: which destination planes receive the image's planes. Scanned from the
  LSB; the image's plane 0 goes to the lowest set bit, plane 1 to the next, etc. `0x0F`
  = "my four planes go into destination planes 0,1,2,3".
- **`PlaneOnOff`**: for destination planes *not* picked, fill the image's rectangle with
  all-0s (bit clear) or all-1s (bit set).

This is a memory-saving device: if your image only uses colours 2 and 3 in a 5-plane
screen, store *one* plane of data, set `PlanePick = 0x02`, `PlaneOnOff = 0x01`.
`PlanePick = 0` with `PlaneOnOff = n` gives you a solid rectangle in pen `n` with no
image data at all.

### (c) Alias a one-plane BitMap over the plane you want

```c
struct BitMap one;
InitBitMap(&one, 1, WIDTH, HEIGHT);
one.Planes[0] = big.Planes[4];
/* now a RastPort on &one draws into plane 4 with pen 0/1 semantics */
```

Useful for masks, collision maps, or reusing plane-agnostic code.

### (d) Poke the memory directly

`plane[y * BytesPerRow + (x >> 3)] |= 0x80 >> (x & 7);` — fastest, no OS involvement,
and no clipping, no layer, no safety net. Fine when you own the whole screen; a good way
to trash someone else's window otherwise.

### The high-plane-as-selector trick

Worth internalising because it's cheap and looks like magic. If plane 4 holds only the
background, then every pixel where background and foreground overlap gets colour index
`n + 16`. By designing entries **16–31 as a deliberate variant of 0–15**, you get
per-pixel compositing for free:

- Boing's entries 18–31 are *identical* to 2–15 → the ball fully **hides** the wall.
- Entries 0/1 are grey bg / grey shadow; 16/17 are purple grid line / dark purple →
  the shadow correctly **darkens the grid lines it falls across**.

Zero per-pixel work. Just palette design.

---

## 4. Animation techniques, cheapest first

### (a) Scroll the ViewPort — no drawing at all *(the Boing method)*

Make the BitMap **bigger than the display**, then move the window onto it:

```c
Screen->ViewPort.RasInfo->RxOffset = x;
Screen->ViewPort.RasInfo->RyOffset = y;
MakeScreen(Screen);      /* rebuild this screen's copper list from its ViewPort */
RethinkDisplay();        /* merge all screens' copper lists and install */
WaitTOF();               /* pace to the 50 Hz PAL frame */
```

`RxOffset`/`RyOffset` (in `struct RasInfo`) select which BitMap pixel sits at the
ViewPort's top-left. Changing them costs a copper-list rebuild and nothing else — no
blitter, no CPU pixel work. This is why Boing runs at full frame rate on a 7 MHz 68000.

At the graphics level the equivalent call is `ScrollVPort(&vp)`. For an **Intuition**
screen, use `MakeScreen()` + `RethinkDisplay()` so Intuition stays in sync with what the
hardware is doing.

**The 16-pixel granularity gotcha.** Bitplane pointers (BPLxPTH/L) address **words**, so
you can only shift a plane horizontally by whole 16-pixel steps via pointer arithmetic:

```c
plane_ptr -= (x >> 4) * 2;   /* coarse: 16 px per 2 bytes */
plane_ptr -= y * BytesPerRow; /* vertical is easy: any number of lines */
```

The leftover 0–15 pixels **cannot be expressed as an address**. Boing's answer: keep
**16 pre-shifted copies** of the plane, each drawn one pixel further right, and select
`copies[x & 15]`. Costs 16× the memory of that plane. The general alternatives are the
hardware scroll registers (BPLCON1, ±15 px fine scroll, not exposed through the OS) or
just re-blitting.

### (b) Palette cycling — animation with no pixels moving

Rotate which colour registers hold which RGB values. Boing's ball "spins" purely this
way: the checker pattern is static image data; only the palette moves.

**The approved API is `LoadRGB4(&vp, table, count)`** (max 32 entries under V37) or
`SetRGB4(&vp, n, r, g, b)`. Boing instead grabs a raw pointer:

```c
ColorTable = (WORD *)Screen->ViewPort.ColorMap->ColorTable;  /* NOT approved */
ColorTable[n] = 0xFFF;
MakeScreen(Screen); RethinkDisplay();
```

This works because `MakeScreen()` regenerates the copper list from the ColorMap
afterwards. It is faster than 32 `SetRGB4()` calls but it reaches into a structure the
OS owns. **Prefer `LoadRGB4()` unless you have measured that you can't afford it.**
(`SetRGB4CM()` writes the ColorMap without touching the copper list, which is the
"official" version of what Boing is doing by hand.)

### (c) Double buffering — swap the plane pointers

Draw into an off-screen BitMap, then flip:

```c
/* two BitMaps, bm[0] and bm[1]; toggle each frame */
Screen->RastPort.BitMap        = &bm[draw];   /* draw here */
Screen->ViewPort.RasInfo->BitMap = &bm[show]; /* show this */
MakeScreen(Screen);
RethinkDisplay();
WaitTOF();
```

Costs 2× the bitmap memory. Under V39+ there is `AllocScreenBuffer()`/`ChangeScreenBuffer()`,
which is the clean way — **but that is 3.0-only and not available to us on 2.05.**

### (d) Blitter / GELs — when things must move *independently*

`BltBitMapRastPort()`, `BltMaskBitMapRastPort()` (with a mask plane, for shaped sprites),
`ClipBlit()` (respects layers — the safe one inside a window). Above that sits the GELs
system: **VSprites** (virtual sprites), **Bobs** (blitter objects, with `SAVEBACK` to
restore background automatically), and **AnimObs/AnimComps** for sequenced animation.
See RKM "Graphics Sprites, Bobs and Animation". Heavier, but it's what you want for
several independently-moving objects.

### (e) Hardware sprites

8 sprites, 16 px wide, 3 colours + transparent (or 15 colours if attached in pairs).
Data must be chip RAM. `SimpleSprite` / `GetSprite()` / `ChangeSprite()` / `FreeSprite()`.
The mouse pointer *is* sprite 0 — `SetPointer(win, data, height, width, xoff, yoff)`
is how Boing replaces it with a single dot.

**Which to use:** viewport scroll for whole-screen motion; sprites for a few small
things over a background; blitter/Bobs for many things or things wider than 16 px;
palette cycling for anything you can fake with colour.

---

## 5. Opening a custom screen

### The 1.x way (`NewScreen`) — still works under 2.05

```c
struct NewScreen ns = {
    0, 0, 320, 200, 5,          /* Left, Top, Width, Height, Depth */
    0, 1,                       /* DetailPen, BlockPen */
    0,                          /* ViewModes: 0 = lores non-interlaced */
    CUSTOMSCREEN | CUSTOMBITMAP,/* Type + flags */
    NULL,                       /* Font (NULL = default) */
    NULL,                       /* Title */
    NULL,                       /* Gadgets — unused, must be NULL */
    &BitMap                     /* CustomBitMap (only if CUSTOMBITMAP) */
};

Screen = OpenScreen(&ns);
```

- **`Depth` must match your BitMap's depth.** No sanity check will save you.
- **`CUSTOMBITMAP` means Intuition allocates nothing and frees nothing.** The planes are
  yours to allocate before `OpenScreen()` and free *after* `CloseScreen()`.
- Without `CUSTOMBITMAP`, Intuition allocates the bitmap for you — which is what you
  want 90% of the time.
- `ViewModes` is `HIRES`, `LACE`, `HAM`, `EXTRA_HALFBRITE`, `SPRITES`, `DUALPF`… from
  `<graphics/view.h>`. **0 = lores, non-interlaced** — on a PAL machine that's 320×256
  addressable, so 320×200 fits with room to spare.

### The 2.0 way (`OpenScreenTagList`) — prefer this on V37

```c
struct Screen *scr = OpenScreenTags(NULL,
    SA_Width,     320,
    SA_Height,    200,
    SA_Depth,     5,
    SA_DisplayID, PAL_MONITOR_ID | LORES_KEY,   /* <graphics/displayinfo.h> */
    SA_Type,      CUSTOMSCREEN,
    SA_Title,     (ULONG)"My Screen",
    SA_Pens,      (ULONG)pens,                  /* -1 terminated UWORD array */
    SA_BitMap,    (ULONG)&BitMap,               /* implies CUSTOMBITMAP */
    TAG_END);
```

Advantages: `SA_DisplayID` picks a mode explicitly instead of relying on `ViewModes`
bits (so you're not silently at the mercy of NTSC/PAL defaults), and `SA_Pens` tells
2.0's rendering which pens to use for the title bar etc. Tags you don't set take
sensible defaults. Cost: nothing — `OpenScreenTagList()` is V36+, well inside our target.

### And a window on it

You still need a **Window** to receive IDCMP messages — screens don't have a message
port. Boing opens a full-screen `BORDERLESS` window purely as an event source (and
paints over it immediately). `RMBTRAP` stops the right button opening menus.

### Teardown order — always the exact reverse

```
CloseWindow(win);        /* windows before their screen, always */
CloseScreen(scr);        /* releases the ViewPort; stops the copper touching planes */
FreeRaster(...) / FreeMem(planes, ...);   /* only NOW is the plane memory idle */
CloseLibrary(GfxBase);
CloseLibrary(IntuitionBase);
```

Freeing bitplanes **before** `CloseScreen()` means the copper is DMA-ing from memory that
now belongs to someone else. On a real machine that's a screenful of somebody else's
data, or a Guru.

---

## 6. Recurring discipline (the stuff that actually bites)

- **`OpenLibrary("intuition.library", 37L)`** — ask for the version you need. `0` means
  "any", which just moves the crash later.
- **Never touch an `IntuiMessage` after `ReplyMsg()`.** Copy `Class` and `Code` into
  locals first. The message is Intuition's again the instant `ReplyMsg()` returns.
- **Drain the port with `while`, not `if`** — one `GetMsg()` per frame silently queues
  up a backlog of clicks.
- **`Wait(1L << win->UserPort->mp_SigBit)`** when idle. Never busy-wait on a 7 MHz 68000.
- **Free in exactly the reverse order you allocated.** A single cleanup routine that is
  safe to call from *any* stage of setup (all pointers NULL-initialised, every free
  guarded) is the pattern worth copying from `boing5.c` — that part it gets right.
- **`MEMF_CHIP` for anything the custom chips read**: bitplanes, sprites, `struct Image`
  data, audio samples, blitter masks. SAS/C's `__chip` keyword puts static data there.
- **Check every return.** `AllocMem`, `AllocRaster`, `OpenScreen`, `OpenWindow`,
  `OpenLibrary`, `OpenDevice` can all fail on a 2 MB machine, and *will*, eventually.
- **`WaitBlit()`** before touching memory the blitter was writing. The blitter is
  asynchronous; `SetRast()` and friends can return before the blit has finished.

---

## 7. Things that are 3.x-only (don't reach for these on 2.05)

- `AllocScreenBuffer()` / `FreeScreenBuffer()` / `ChangeScreenBuffer()` — V39.
- 8 bitplanes / 256 colours, `SetRGB32()`, 24-bit palettes — AGA + V39.
- BOOPSI `ReAction` gadget classes — 3.x.
- `WriteChunkyPixels()` — V40.

For 2.05, the toolkit is: `NewScreen`/`OpenScreenTags`, `GadTools` for gadgets and menus,
manual double buffering by pointer swap, `LoadRGB4()` for palettes, and the blitter.
