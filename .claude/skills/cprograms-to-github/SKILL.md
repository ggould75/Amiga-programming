---
name: cprograms-to-github
description: Pull C source from the Amiga "C_Programs" .adf into this repo's C_Programs/ folder (whole disk, all .c files and subfolders). Use when the user added or modified .c files on the C_Programs disk and wants them on GitHub. Read-only on the disk; stops for the user to review and commit. Amiga→GitHub only.
---

# cprograms-to-github

Brings changes from the **`C-Programs.adf`** disk (volume `C_Programs`) into the repo mirror
`C_Programs/`. Covers the **whole disk** (root + subfolders, e.g. `Intuition/`), all `.c` files.
**Read-only on the disk**, Amiga→GitHub only. Never auto-commits.

Begin by sourcing the shared lib:

```bash
source ~/Dev/Amiga-programming/.claude/skills/adf-sync-lib/adf-lib.sh
adf_require_xdftool || exit 1
adf_require_mounted || exit 1   # USB at /Volumes/GOTEK/... — fail fast up front
DISK="C_Programs"; MIRROR="$REPO/C_Programs"
```

## Steps

1. **Pull and unpack** (read-only):
   ```bash
   WORK="$(adf_pull "$DISK")" || exit 1
   adf_unpack "$WORK" /tmp/cp-pull
   ```

2. **Find all `.c` on the disk** (root + subfolders) and compare to the repo mirror, preserving
   relative paths (`hello.c` → `C_Programs/hello.c`, `Intuition/x.c` → `C_Programs/Intuition/x.c`):
   ```bash
   ( cd /tmp/cp-pull && find . -name '*.c' )   # rel paths from disk root
   ```
   - If the user named specific file(s), limit to those.
   - Classify each as NEW or MODIFIED vs `$MIRROR/<rel>`. Show diffs + new-file list.

3. **Copy changed/new files into the repo mirror**, recreating subfolders:
   ```bash
   mkdir -p "$MIRROR/$(dirname "$rel")"
   cp "/tmp/cp-pull/$rel" "$MIRROR/$rel"
   ```
   Don't delete repo files absent on disk unless the user explicitly asks.

4. **Keep the root `README.md` "Programs" section in sync.** The repo `README.md`
   documents every program under `C_Programs/` (2–3 lines each: what it does + which
   libraries it uses). Whenever this sync **adds** a program, **removes** one (only if
   the user asked to delete it), or a **modified** file changes materially (new
   library opened, different API/technique, different purpose or CLI flags), update
   that section to match:
   - New program → add an entry in the right subsection (e.g. `Intuition/`), following
     the existing bullet style, reading the source to describe it accurately.
   - Removed program → delete its entry.
   - Materially changed program → revise its blurb (esp. the *Libraries:* line).
   - Pure edits with no behavioural/library change (typo, refactor, comments) → leave
     the README as is.
   Read the actual source before writing a blurb — don't guess. Keep it tidy and
   consistent with the entries already there.

5. **STOP. Summarize and let the user review + commit.** Do not run git; suggest `/commit`.
   Note in the summary whether the README was updated. Optionally eject: `adf_eject || true`.

## Hard rules
- Read-only on the disk; never write the .adf.
- Never commit/push automatically — always stop for user review.
- Only ignore build artifacts (`.o`, `.lnk`, executables, `.info`); sync `.c` only.
- The `README.md` "Programs" section covers `C_Programs/` only; keep it current (step 4).
