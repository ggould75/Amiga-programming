#!/usr/bin/env bash
# Shared helpers for the Amiga <-> GitHub ADF sync skills.
# Source this file:  source ~/Dev/Amiga-programming/.claude/skills/adf-sync-lib/adf-lib.sh
# Do NOT use `set -e` here; functions signal failure via return codes.

# ---- config ----------------------------------------------------------------
XDF="${XDF:-$HOME/Library/Python/3.9/bin/xdftool}"
GOTEK_DIR="${GOTEK_DIR:-/Volumes/GOTEK/Installed/SAS:C v6.0}"
REPO="${REPO:-$HOME/Dev/Amiga-programming}"
XFER="${XFER:-$HOME/Downloads/amiga-adf-transfer}"

# ---- disk registry ---------------------------------------------------------
# Map a disk key to its properties. Add new disks here only.
adf_file_for() {    # disk key -> .adf filename on the USB
  case "$1" in
    "SAS-C Disk 1") echo "SAS-C Disk 1.adf" ;;
    "C_Programs")   echo "C-Programs.adf" ;;
    *) return 1 ;;
  esac
}
repo_mirror_for() { # disk key -> repo mirror root (relative to $REPO)
  case "$1" in
    "SAS-C Disk 1") echo "SAS-C Disk 1" ;;
    "C_Programs")   echo "C_Programs" ;;
    *) return 1 ;;
  esac
}
scope_for() {       # disk key -> synced subfolder ("" = whole disk)
  case "$1" in
    "SAS-C Disk 1") echo "Programs" ;;
    "C_Programs")   echo "" ;;
    *) return 1 ;;
  esac
}
is_critical() {     # disk key -> "yes"/"no"
  case "$1" in
    "SAS-C Disk 1") echo "yes" ;;
    *) echo "no" ;;
  esac
}

# ---- preconditions ---------------------------------------------------------
adf_require_xdftool() {
  if [ ! -x "$XDF" ]; then
    echo "ERROR: xdftool not found at $XDF (install amitools, or set \$XDF)" >&2
    return 1
  fi
}
adf_require_mounted() {
  if [ ! -d "$GOTEK_DIR" ]; then
    echo "ERROR: Gotek not mounted ($GOTEK_DIR missing). Plug the USB into the Mac." >&2
    return 1
  fi
}

# ---- pull / deploy / eject -------------------------------------------------
# adf_pull <diskkey> : copy live USB image into work/ and backup/before/. Echoes work-image path.
adf_pull() {
  local key="$1" file work
  file="$(adf_file_for "$key")" || { echo "ERROR: unknown disk '$key'" >&2; return 1; }
  adf_require_mounted || return 1
  mkdir -p "$XFER/work" "$XFER/backup/before" "$XFER/backup/after"
  work="$XFER/work/$file"
  cp "$GOTEK_DIR/$file" "$work" || return 1
  cp "$GOTEK_DIR/$file" "$XFER/backup/before/$file" || return 1
  echo "$work"
}

# adf_deploy <workimg> <diskkey> : temp+rename onto USB, then snapshot to backup/after.
adf_deploy() {
  local work="$1" key="$2" file dest
  file="$(adf_file_for "$key")" || return 1
  adf_require_mounted || return 1
  dest="$GOTEK_DIR/$file"
  cp "$work" "$dest.tmp" || return 1
  sync
  mv "$dest.tmp" "$dest" || return 1
  sync
  cp "$work" "$XFER/backup/after/$file" || return 1
  echo "deployed $file"
}

adf_eject() {
  if diskutil eject /Volumes/GOTEK 2>/dev/null; then
    echo "ejected"
  else
    echo "EJECT BLOCKED — a shell is sitting in /Volumes/GOTEK. cd out of it, then: diskutil eject /Volumes/GOTEK" >&2
    return 1
  fi
}

# ---- read-side helpers (never modify the disk) -----------------------------
adf_unpack() { "$XDF" "$1" unpack "$2" >/dev/null; }   # adf_unpack <workimg> <destdir>
adf_info()   { "$XDF" "$1" info; }

# ---- write-side helpers ----------------------------------------------------
# adf_delete_artifacts <workimg> <diskdir> <stem> : remove stale <stem>.o/.lnk/<stem>
adf_delete_artifacts() {
  local work="$1" dir="$2" stem="$3" a
  for a in "$stem.o" "$stem.lnk" "$stem"; do
    "$XDF" "$work" delete "$dir/$a" >/dev/null 2>&1 || true
  done
}

# adf_write_verify <workimg> <diskpath> <hostfile> : delete-then-write, then read-back diff.
# Returns 0 only if the file on the image is byte-identical to <hostfile>.
adf_write_verify() {
  local work="$1" diskpath="$2" host="$3" back
  "$XDF" "$work" delete "$diskpath" >/dev/null 2>&1 || true
  "$XDF" "$work" write "$host" "$diskpath" || { echo "ERROR: write failed ($diskpath) — disk may be full" >&2; return 1; }
  back="$(mktemp)"
  "$XDF" "$work" read "$diskpath" "$back" >/dev/null || { rm -f "$back"; return 1; }
  if diff -q "$host" "$back" >/dev/null; then rm -f "$back"; return 0; fi
  echo "ERROR: read-back mismatch for $diskpath" >&2; rm -f "$back"; return 1
}
