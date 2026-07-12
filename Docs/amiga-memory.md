# Amiga memory — chip, fast, and why it matters

Notes on how memory works on the Amiga: what the three kinds are, which chips can reach
which, and the rules that follow. Written with the **A500 Plus (OS 2.05, ECS, PAL,
68000, SAS/C 6.0)** in mind, but the model is general.

What's in here:

- **The one sentence** that explains every chip-RAM rule.
- **The three kinds of RAM** — chip, fast, and the peculiar "slow" RAM.
- **Why fast RAM is fast** — it's about bus contention, not chip speed.
- **The struct/data split** — what actually has to be in chip RAM, and what doesn't.
- **The `MEMF_` flags** and the allocation rules that follow from them.
- **Reference tables** — the address map, and Agnus revisions vs. chip RAM limits.
- **The trap this specific machine sets**, and how not to fall in it.

---

## 0. Watch this first

Fragment from the 8-bit guy:[
Commodore History Part 8-The Amiga 1000](https://youtu.be/kjapiUQOi2s?t=348)

## 1. One sentence explains almost everything

> **Agnus generates every DMA address on the Amiga, and Agnus can only reach chip RAM.**

Another fragment from the 8-bit guy:[
Commodore History Part 8-The Amiga 1000](https://youtu.be/kjapiUQOi2s?t=1000)



Everything else follows.

- **Denise** paints the screen but never fetches anything — Agnus hands her the data.
- **Paula** plays sound but never fetches anything — Agnus hands her the samples.
- The **blitter** and the **copper** literally live *inside* Agnus.

So bitplanes, sprite data, audio samples, copper lists, and blitter masks must all be in
chip RAM — not by convention, not as an optimisation, but because Agnus is the one going
to fetch them and chip RAM is the only place her address lines reach.

| Chip | Job | Memory it can reach |
|---|---|---|
| **Agnus** | DMA controller and address generator; contains the blitter and the copper | **Chip RAM only** |
| **Denise** | Turns bitplane/sprite data into video; holds the colour registers | **None** — Agnus feeds her |
| **Paula** | Audio, floppy, serial, interrupts | Chip RAM, but **via Agnus** |
| **Gary / Gayle / Buster** | Glue: bus arbitration, address decoding | (decode only) |
| **68000** | You | **Everything** |

---

## 2. The three kinds of RAM

**Chip RAM** is the shared workspace. Both the CPU and the custom chips can use it. It is
the only memory that can be *displayed* or *played*.

**Fast RAM** is CPU-only territory. The custom chips cannot see it at all. Zorro II
boards, accelerator RAM, Zorro III on the big machines.

**Slow RAM** is the odd one out — the A501-style trapdoor expansion on a *plain* A500,
living at `$C00000`. Exec reports it as `MEMF_FAST`, and the custom chips indeed can't
DMA from it… but it's wired to the chip bus anyway, so it's exactly as slow as chip RAM.
All of the drawbacks, none of the benefits. Everyone calls it "slow fast RAM" and means
it unkindly. It's an addressing shortcut in the original Fat Agnus, and it's the reason
some A500 benchmarks show no speedup at all from "fast" RAM.

---

## 3. Why "fast" RAM is fast

Not because the chips are quicker. **It's about who else wants the bus.**

Picture one scanline. Agnus doles out memory access slots in a fixed priority order —
refresh, disk, audio, sprites, bitplanes — and whatever is left over goes to the blitter
and the CPU. With a shallow display, plenty is left over and the 68000 runs at full
speed. Crank the display up to 5 or 6 bitplanes, or go hires, and bitplane fetching
swallows nearly every slot. Now the CPU is standing in a queue.

Which gives the counterintuitive rule:

> **A deeper screen makes your code run slower.** Not just the drawing — *all* of it,
> including logic that has nothing to do with graphics.

Fast RAM sidesteps this completely: nothing is competing for it. Same silicon, no queue.
So on a machine that has it, put code, stack, and general data structures in fast RAM,
and reserve chip RAM for the things that genuinely must live there.

(This is part of what makes Boing clever. A 5-plane display is expensive in exactly this
way — but Boing barely uses the CPU at all. The copper does the work.)

---

## 4. The struct/data split

Only the bytes a chip actually **fetches** need to be chip RAM. The C structure that
*describes* them is read by the CPU, so it can live anywhere.

```c
struct BitMap *bm = AllocMem(sizeof(struct BitMap), MEMF_CLEAR);  /* anywhere */
bm->Planes[0] = AllocRaster(WIDTH, HEIGHT);                       /* CHIP */
```

The same pattern all the way down:

| Must be chip | Can be anywhere |
|---|---|
| `BitMap.Planes[n]` — the plane memory | `struct BitMap` |
| `Image.ImageData` | `struct Image` |
| Sprite image data | `struct SimpleSprite` |
| Audio sample bytes | `struct IOAudio` |
| Copper instruction list | `struct View`, `struct ViewPort` |
| Blitter masks, `TmpRas` raster | `struct RastPort`, `struct AreaInfo` |

`AllocRaster()` gives you chip RAM automatically — that's most of what it's for. So does
SAS/C's `__chip` keyword for static data:

```c
__chip UWORD ball_bitplanes[] = { 0x0000, 0x1234, /* ... */ };
```

Convenient, but `__chip` data is fixed at link time and can never be freed. For anything
large, prefer `AllocMem(size, MEMF_CHIP | MEMF_CLEAR)` and load from disk.

---

## 5. The `MEMF_` flags

From `<exec/memory.h>`.

| Flag | What it means |
|---|---|
| `MEMF_ANY` (`0`) | "You pick." Exec prefers **fast RAM** if any exists. Usually what you want. |
| `MEMF_CHIP` | The custom chips must be able to reach it. Non-negotiable for §4's left column. |
| `MEMF_FAST` | Must **not** be chip. **Fails outright on a machine with no fast RAM.** Almost never the right call. |
| `MEMF_PUBLIC` | Must stay valid while another task runs — message ports, IORequests, task structures. Cheap; harmless when unnecessary. |
| `MEMF_CLEAR` | Zero the block. A *modifier*, not a place. |
| `MEMF_LOCAL` | Survives a reset (V36+). |
| `MEMF_LARGEST` | Only for `AvailMem()` — see below. |

Two useful diagnostics:

```c
ULONG TypeOfMem(APTR address);       /* what kind of memory is this, really? */
ULONG AvailMem(ULONG requirements);  /* AvailMem(MEMF_CHIP | MEMF_LARGEST) */
```

`TypeOfMem()` makes a good development-time assertion: if you *think* something is in
chip RAM, ask it.

### Allocation habits

Exec's allocator is a **first-fit free list**. No compaction, no virtual memory, and
fragmentation is permanent within a session. You can have 300K free and still fail to get
a 200K bitmap, because it's in pieces.

- Allocate the big things **early**, allocate them **once**, and don't churn.
- `AvailMem(MEMF_CHIP)` tells you a hopeful total. `AvailMem(MEMF_CHIP | MEMF_LARGEST)`
  tells you the truth — the biggest *contiguous* block.
- Pair every allocation with its free, in reverse order.

---

## 6. Reference: the address map

The 68000 has a 24-bit address bus — **16 MB**, `$000000`–`$FFFFFF`, carved up in
hardware:

| Range | What it is |
|---|---|
| `$000000`–`$1FFFFF` | **Chip RAM** (up to 2 MB, Agnus-dependent) |
| `$200000`–`$9FFFFF` | Zorro II autoconfig — **fast RAM** boards and other cards |
| `$BFD000` / `$BFE001` | **CIA-B** / **CIA-A** (keyboard, timers, floppy, ports) |
| `$C00000`–`$D7FFFF` | **Slow RAM** (trapdoor / "ranger") |
| `$DFF000`–`$DFF1FF` | **Custom chip registers** |
| `$E80000`–`$EFFFFF` | Autoconfig space |
| `$F80000`–`$FFFFFF` | **Kickstart ROM** (512K under 2.0+) |

Two things worth noticing:

- **`$000000` — the 68000 exception vector table — is inside chip RAM.** This is why a
  wild pointer write can take the whole machine down.
- **Custom registers are memory-mapped.** `*(UWORD *)0xDFF180 = 0x0F00;` really does turn
  the background red. That's "banging the hardware" — and it's also why the OS gets very
  unhappy when you do it behind its back.

## 7. Reference: Agnus revisions vs. chip RAM

| Agnus | Max chip RAM | Typical machine |
|---|---|---|
| 8361 / 8370 / 8371 (Fat) | 512 K | A1000, early A500/A2000 |
| 8372A (ECS) | 1 MB | A500 rev 6, A2000, A3000 |
| 8372B / 8375 (ECS) | 2 MB | **A500 Plus**, A600 |
| 8374 Alice (AGA) | 2 MB | A1200, A4000 |

**No Amiga ever made has more than 2 MB of chip RAM.** (Exact part-number-to-capacity
mapping gets fiddly across board revisions — worth checking a hardware reference before
relying on the details.)

---

## 8. The trap this machine sets

The A500 Plus has an ECS Agnus addressing 2 MB, and its trapdoor slot is wired so the
expansion becomes **more chip RAM**, not slow RAM. So the 2 MB is *all chip*. There is no
fast RAM anywhere in the machine.

Which means:

> **Every allocation on this machine is chip RAM, whether you ask for it or not. So
> forgetting `MEMF_CHIP` works perfectly — here.**

Then the binary runs on an A1200 with a fast-RAM board. The allocator cheerfully hands out
fast RAM. Agnus fetches from an address she cannot see. Screen of noise, silent audio,
and a bug that is **structurally impossible to reproduce on the machine it was written
on.**

So: be explicit about `MEMF_CHIP` even though, today, it changes nothing.

Corollary, in the other direction: **never ask for `MEMF_FAST` on this machine** — the
allocation will simply fail. Use `MEMF_ANY` and let Exec decide.

*(Note: some A500 Plus boards carry a jumper — JP2 — that forces the trapdoor to appear
as slow RAM at `$C00000` for 1.3 compatibility. Worth checking if the memory figures ever
look wrong.)*
