---
name: amiga-to-github
description: Pull C source from the Amiga "SAS-C Disk 1" .adf (Programs/ folder only) into this repo. Use when the user added or modified .c files on the Amiga (disk 1) and wants them on GitHub. Read-only on the disk; stops for the user to review and commit.
---

# amiga-to-github

Brings changes from **`SAS-C Disk 1.adf`** `Programs/` into the repo mirror
`SAS-C Disk 1/Programs/`. **Read-only on the disk** — never writes the .adf. Never auto-commits.

Begin by sourcing the shared lib:

```bash
source ~/Dev/Amiga-programming/.claude/skills/adf-sync-lib/adf-lib.sh
adf_require_xdftool || exit 1
adf_require_mounted || exit 1   # USB at /Volumes/GOTEK/... — fail fast up front
DISK="SAS-C Disk 1"; MIRROR="$REPO/$DISK/Programs"
```

## Steps

1. **Pull the live disk to a working copy** and unpack it (read-only):
   ```bash
   WORK="$(adf_pull "$DISK")" || exit 1
   adf_unpack "$WORK" /tmp/adf-pull
   ```

2. **Compare `Programs/*.c` on disk vs the repo mirror.**
   - If the user named specific file(s), limit to those.
   - Otherwise consider every `*.c` under `/tmp/adf-pull/Programs`.
   - For each: NEW (not in `$MIRROR`) or MODIFIED (differs from `$MIRROR/<f>`). Show the diffs and
     the new-file list.

3. **Copy changed/new files into the repo mirror:**
   ```bash
   mkdir -p "$MIRROR"
   cp "/tmp/adf-pull/Programs/$f" "$MIRROR/$f"
   ```
   Only files under `Programs/`. Do not delete repo files just because they're absent on disk
   unless the user explicitly asks (a file may have been moved to another disk).

4. **STOP. Summarize what changed and let the user review + commit.** Do not run git.
   Suggest they run `/commit` (and rebuild/verify on the Amiga is their call). Optionally eject:
   ```bash
   adf_eject || true
   ```

5. **Source-of-truth reminder — ALWAYS output this last, verbatim and prominent**, as the very
   final thing in your response (after the summary), so the user records which copy is now
   authoritative and the copies don't silently diverge:
   ```
   ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
   ┃ ⚠️  WRITE DOWN THE MOST UPDATED SOURCE OF TRUTH NOW.       ┃
   ┃ WHICH COPY HOLDS THE LATEST WORK?                         ┃
   ┃ AMIGA/GOTEK USB · MAC .ADF · FS-UAE .HDF · GITHUB          ┃
   ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
   ```

## Hard rules
- Scope is `Programs/` only.
- Never write the .adf or the USB; this direction is read-only on the disk.
- Never commit/push automatically — always stop for user review.
- Always end with the uppercase source-of-truth reminder (step 5) — never skip it.
