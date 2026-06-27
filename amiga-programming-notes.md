# Amiga Programming Notes

## `<clib/X_protos.h>` vs `<proto/X.h>`

`<clib/X_protos.h>` gives only the function **prototypes**. `<proto/X.h>` is the
superset: prototypes + the **calling convention** (pragmas on SAS/C, inlines on
GCC) + the **library base declaration** (`extern struct Library *LayersBase;`).
So once you include `<proto/X.h>`, the matching `<clib/*_protos.h>` is redundant
(NDK chain: proto → pragmas → clib).

### Example — SAS/C pragma

```c
#pragma libcall LayersBase NewLayerInfo 2a 0
/*              base        function    LVO=0x2a, no register args */
```
The compiler reads this and emits `jsr -0x2a(a6)` for a plain `NewLayerInfo()`
call. (GCC instead ships a `static inline` wrapper with the same `jsr` in
inline asm.)

## `__OSlibversion` when using auto-opening of libraries

With `<proto/*.h>`, the SAS/C startup auto-opens any library base that's
**declared but not defined**. By default it opens *any* version, so a program
using V37-only calls (e.g. `NewLayerInfo`/`DisposeLayerInfo`) would open fine on
a 1.3 ROM and then crash. Setting the floor makes the startup fail cleanly
instead:

```c
long __OSlibversion = 37;     /* require OS 2.04 / V37 */
```

Casing matters: it's `__OSlibversion` (two underscores, capital `OS`, lowercase
`libversion`) — `__OSLibversion` is a different, unused name. Not needed when you
`OpenLibrary("...", 37)` manually, since the version arg already enforces it.

