# Amiga ⇄ FS-UAE ⇄ GitHub sync checklist

A step-by-step routine for moving work between the real Amiga (Gotek USB),
FS-UAE (Workbench 3.1 + SAS/C on the `.hdf`), and GitHub — without losing or
corrupting anything.

## Golden rule

**The `.adf` is the master. Only one copy is "live" at a time.** Before you edit
at a station, refresh the newest `.adf` *in*; when you finish, push it *out* and
back it up to GitHub. Never edit the same disk at two stations without syncing in
between.

Two *kinds* of copy — don't mix them up:

- **Whole-`.adf` file copy** (USB ↔ Mac FS-UAE folder) → the transport. Preserves
  the exact image, including compiled binaries.
- **`.c` extraction skill** (`.adf` → repo) → *only* to feed GitHub as a
  backup/viewer. It never replaces the transport copy.

Stations:

- 🟠 **Amiga** — disk = `.adf` on the Gotek USB
- 💻 **FS-UAE** — disk = `.adf` in a fixed Mac floppy folder
- ☁️ **GitHub** — extracted `.c`, backup only

---

## 🟠 Weekend — working on the Amiga

1. USB in the Gotek. Edit/compile your `.c` files.
2. Before power-off: let the drive finish writing (back to Workbench, drive light
   off), then shut down cleanly.

## ☁️ Before leaving home — back up + load the Mac

3. Move USB from Gotek → Mac.
4. Run the sync skill (`/cprograms-to-github` for C-Programs, `/amiga-to-github`
   for SAS-C Disk 1). Review the diff, **commit & push** → GitHub current.
5. **Copy the whole `.adf`** from the USB into your FS-UAE floppy folder
   (overwrite the old one). That exact file is what FS-UAE mounts.
6. Eject the USB.

## 💻 During the week — working in FS-UAE

7. Boot FS-UAE from the `.hdf` (WB 3.1 + SAS/C).
8. Insert the `.adf` from the floppy folder via swap list (**F12**) —
   **writable, NOT write-protected**.
9. Edit / compile (send SAS/C temp & objects to `RAM:`/hard disk to save floppy
   space).
10. Done for the day: **eject the floppy in FS-UAE (or quit FS-UAE cleanly)** so
    writes flush to the `.adf`. **Never copy the `.adf` while FS-UAE is running.**
11. *(Optional backup)* extract the Mac `.adf`'s `.c` and push to GitHub. Note:
    the current skills read from the USB, not a Mac `.adf`, so this needs a small
    skill extension before it's one command.

## 🟠 Getting home — bring the Amiga current FIRST

12. Plug USB into the Mac.
13. **Copy the FS-UAE floppy `.adf` → the USB `.adf`** (overwrite). Now the USB
    carries your week's work.
14. *(Optional)* run the sync skill and push so GitHub matches too.
15. Eject USB → Gotek → boot the Amiga. You're continuing on the latest.
    **Do not boot the Amiga on the old USB before doing step 13.**

---

## Safety checks (10 seconds each)

- **Which is newest?** `ls -l` both `.adf` files and compare the mtime, or
  `shasum` both to see if they differ.
- **Commit to GitHub before overwriting any `.adf`** — that's your recovery
  snapshot for the `.c` files.
- **Copy only when nothing has it open** (Amiga powered down / FS-UAE floppy
  ejected).
- If you ever lose track: whichever `.adf` you *last edited* wins; GitHub's last
  commit is the fallback.
