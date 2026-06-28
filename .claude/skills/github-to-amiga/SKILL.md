---
name: github-to-amiga
description: Push C source from this repo onto the Amiga "SAS-C Disk 1" .adf on the Gotek USB (Programs/ folder only). Use when the user wants to update/write one or more .c files (or "whatever changed") from GitHub/repo to the Amiga disk. This is the only skill that WRITES the system-critical install disk.
---

# github-to-amiga

Writes repo source into **`SAS-C Disk 1.adf`**, scope **`Programs/` only**. This disk is the
SAS/C install disk — never touch anything outside `Programs/`.

Always begin by sourcing the shared lib (provides config, registry, safe helpers):

```bash
source ~/Dev/Amiga-programming/.claude/skills/adf-sync-lib/adf-lib.sh
adf_require_xdftool  || exit 1   # tool present?
adf_require_mounted  || exit 1   # USB at /Volumes/GOTEK/...  — FAIL FAST here, before any work
DISK="SAS-C Disk 1"; MIRROR="$REPO/$DISK/Programs"
```

Both preconditions are checked **up front**. If the USB isn't mounted at the expected path, the
skill stops immediately with a clear message — it never starts the write workflow half-way.

## Steps

1. **Pull the live disk to a working copy** (also snapshots `backup/before/`) and unpack it so the
   diff uses the actual current disk state:
   ```bash
   WORK="$(adf_pull "$DISK")" || exit 1
   adf_info "$WORK"               # note free space
   adf_unpack "$WORK" /tmp/g2a-before
   ```

2. **Determine the file list.**
   - If the user named specific file(s) (e.g. `layers.c`), use those (basenames under `Programs/`).
   - If not, compute "what differs": for each `*.c` in `$MIRROR`, diff against
     `/tmp/g2a-before/Programs/<f>`; the changed/new ones are the candidates. Show the user the
     list and per-file diffs, and **ask for confirmation** before writing.

3. **For each file to write**, free stale artifacts then write+verify:
   ```bash
   stem="${f%.c}"
   adf_delete_artifacts "$WORK" "Programs" "$stem"   # removes stale Programs/<stem>.o/.lnk/<stem>
   adf_write_verify "$WORK" "Programs/$f" "$MIRROR/$f" || { echo "ABORT: $f failed"; exit 1; }
   ```
   If a write fails for lack of space, STOP and report — do not deploy. (Tell the user they may
   need to move programs to the C_Programs disk to make room.)

4. **Verify the whole change** before deploying:
   ```bash
   adf_unpack "$WORK" /tmp/g2a-after
   diff -r /tmp/g2a-before/Programs /tmp/g2a-after/Programs   # expect ONLY intended .c changes + removed artifacts
   ```
   Show this diff. If anything outside the intended files changed, STOP.

5. **Deploy** (temp+rename onto USB, snapshot to `backup/after/`) and eject:
   ```bash
   adf_deploy "$WORK" "$DISK"
   adf_eject || true   # if blocked, tell the user to cd out of /Volumes/GOTEK and retry
   ```

6. **Tell the user to rebuild on the Amiga.** An ADF round-trip cannot catch compile errors
   (missing headers/constants only surface on the SAS/C compiler). Do NOT commit anything here —
   the source already lives in the repo; committing is a separate user decision.

## Hard rules
- Scope is `Programs/` only. Never read, write, or diff anything else on this disk.
- Never run `xdftool write/delete` against the live USB file — only against `$WORK`.
- Never deploy an image that failed read-back verification.
- Deploy only via temp+rename (handled by `adf_deploy`).
