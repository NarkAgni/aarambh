# Changelog

All notable changes to Aarambh are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.7] - 2026-11-12

Safer font handling, kernel copies that survive a full EFI partition and a
failed scan, and settings that are no longer lost when two commands run at
once. Update with `git pull && make install`, then run `setup.sh` once on
every other Linux.

### Fixed

- A font with correct checksums but a wrongly built table could still crash
  the menu when its first text was drawn: the character map was only checked
  for size, and glyph outlines not at all. Fonts are now checked the way
  stb_truetype reads them: the character map it actually uses, format by
  format; every glyph outline, so no point or flag is read past the glyph's
  own bytes; and composite glyphs, which may not loop or nest deeper than
  eight levels. A font that fails is not used.
- Running out of memory while a glyph was drawn crashed the menu, after the
  backgrounds had loaded fine. A glyph without memory is now skipped, keeps
  its width, and is drawn once memory is back. Glyphs are drawn in strips of
  at most 64 pixels, the width stb_truetype handles without an allocation it
  does not check; the result is identical pixel for pixel.
- A kernel scan that failed partway (an unreadable `/boot`, a failing `sort`,
  unreadable GRUB menu ids, BLS folders that cannot be listed) published an
  empty kernel list and deleted the direct-boot copy. A failed scan now fails
  the run and leaves the last list and its copies as they were.
- With too little room for a second kernel copy, the old copy was deleted
  before the new kernel list was published, so a failure in between left the
  old list pointing at missing files. A copy the published list names is now
  never deleted first: the new kernel is listed for the OS's own loader, that
  list is published, then the old copy goes and the new one is made in a
  second pass.
- A damaged kernel copy on the EFI partition was used again as long as its
  name matched. Copies are now checked before reuse, byte for byte or by
  their signature, and written again when damaged.
- Two commands changing `aarambh.conf` at the same moment (for example
  `aarambh --theme` and `aarambh --timeout`, or a kernel update) could lose
  one change. Every writer now takes one shared lock around reading, changing
  and replacing the file.
- `aarambh --default` accepted a folder under `\EFI\` without a boot loader,
  a configured system whose loader is gone, a system that `autodetect no`
  hides, Restart and Power Off, and indexes past the last system. It now
  accepts exactly what the menu would boot, and lists the entries otherwise.
- A very large timeout overflowed when read at boot. Numbers in
  `aarambh.conf` are now read safely, and the timeout is limited to 999
  seconds, in the menu and in `aarambh --timeout`. The countdown also shows
  three-digit numbers correctly now.

### Changed

- Fonts with CFF outlines (most `.otf` files) are no longer used; set
  TrueType (`.ttf`) fonts in `aarambh.conf`. The bundled fonts are TrueType.
- Fonts whose ascent and descent leave no line height are refused.
- `aarambh --timeout` accepts 0 to 999 and stores the number without leading
  zeros.
- The first `aarambh-kernels` run of this version checks the existing kernel
  copy; a copy made by 0.1.6 that is intact is kept.

### Added

- `make test` runs about 110 more checks: fonts built wrong with correct
  checksums (including 600 rounds of random damage), memory running out at
  every step of drawing text, failing kernel scans, low space with a failing
  list update, damaged kernel copies, `aarambh` commands running at the same
  time, and the limits of `--default` and `--timeout`.

## [0.1.6] - 2026-11-11

A reliability release: eleven bugs found in a review of 0.1.5, all in what
happens when something is missing, damaged or fails halfway. Update with
`git pull && make install`, then run `setup.sh` once on every other Linux.

### Fixed

- A damaged font could crash the menu before it appeared: an empty, cut short
  or wrong file in `fonts/` was handed to the font reader unchecked. Every
  font is now checked first (all tables inside the file and matching their
  checksums); a font that fails is not used and its text is drawn with one of
  the others.
- Running out of memory while loading backgrounds crashed the menu (high
  resolution screens with many entries). A failed allocation now leaves an
  empty image, entries that got no background share a spare one that is
  reserved up front, and when even that does not fit Aarambh exits cleanly so
  the firmware starts the next boot entry.
- A kernel was started directly although its initramfs was missing: when a
  BLS entry named several initrd files and one did not exist, the others were
  used alone (for example only the microcode). Such a kernel now starts
  through the OS's own boot loader.
- `$kernelopts` in a Fedora or RHEL boot entry replaced the whole line, so
  options after it (`systemd.unit=rescue.target`, `rd.break`) were dropped.
  GRUB variables are now filled in where they stand, from `grubenv`, and an
  unset one (`$tuned_params`, `$tuned_initrd`) is empty as in GRUB.
- A kernel command line longer than 767 characters was silently cut, which
  could remove `root=` or the options of an encrypted disk. The limit is now
  2047 characters, the kernel's own; a longer command line, a longer path or
  more than four initramfs parts are never cut: that kernel starts through the
  OS's boot loader and `aarambh-kernels` prints a warning.
- Copying the kernel to the EFI partition reported success when the final
  rename failed, and could leave a new kernel next to the initramfs of the
  old one. Every step is checked now, and the copy is made as a pair
  (`vmlinuz-<n>`, `initrd-<n>.img`) that the kernel list names, so a kernel is
  never started with another kernel's initramfs.
- With no operating system found (wrong EFI partition, deleted loaders) the
  countdown started Restart or Firmware by itself, in a loop. The countdown
  now only ever starts an operating system; otherwise the menu shows "No
  bootable system found" and waits.
- On an EFI partition shared by several systemd-boot installations, a boot
  entry of another system was taken for this one when its `root=` only began
  the same way (`/dev/sda1` and `/dev/sda10`, `LABEL=linux` and
  `LABEL=linux-old`). The argument must now match exactly, and an entry that
  carries another machine id is never taken.
- A kernel picked under Advanced options was not booted when the choice could
  not be stored for GRUB or systemd-boot (full or read-only EFI partition):
  the loader was started anyway and booted its default kernel. Aarambh now
  shows "Could not save the kernel choice, nothing was started" and returns to
  the menu.
- Writing a file from the menu reported success when the old file could not
  be deleted, when only part of it was written or when flushing failed, so a
  one-time kernel choice could stay in place. The file is now truncated
  explicitly and every step counts.
- A kernel list that could not be generated left an empty or half-written
  `<os>.kernels`, and the Advanced options were gone. The list is now written
  next to the old one and replaces it in one step, only when it is complete;
  two kernel hooks that run at the same time wait for each other.
- `aarambh.conf` could be emptied when rewriting it failed (`setup.sh`,
  `uninstall.sh`, `aarambh --theme`); it is replaced in one step now.
- A small memory leak while choosing the screen mode.

### Changed

- The direct-boot kernel copy is named `linux\<os>\vmlinuz-<n>` and
  `initrd-<n>.img` instead of `vmlinuz` and `initrd.img`. The old files are
  removed by the first `aarambh-kernels` run of this version.
- `default restart` (or any other action) no longer starts by itself after the
  countdown.
- The menu writes `next-<os>.cfg` only when the kernel choice changed, not on
  every boot.
- Problems during a kernel update are no longer thrown away: the refresh
  trigger sends them to the system log (`journalctl -t aarambh`) and still
  never fails the package operation. `aarambh-debug` shows them.
- Write-combining is turned on after the menu's files are loaded, so a
  problem while loading them is not recorded as a write-combining failure.

### Added

- README: the installation is now six numbered steps with what you should see
  after each, followed by everyday use, update, uninstall and recovery; the
  details moved into a Reference part. New troubleshooting entries for the
  new messages.
- `make test` also runs `tests/hooks/failures.sh` (failure paths of the
  hooks) and `tests/unit/run.sh` (parts of `aarambh.efi` built for Linux and
  run against mocked UEFI services, with AddressSanitizer when installed).

## [0.1.5] - 2026-11-10

### Fixed

- A first install stopped with "installation did not finish" after copying
  every file, and the firmware boot entry was never created: with no Aarambh
  entry yet, the lookup for it failed under `set -euo pipefail`. A second run
  worked, so only new installs of 0.1.3 and 0.1.4 were hit.
  Contributed by [@DrArzter](https://github.com/DrArzter) in
  [#2](https://github.com/NarkAgni/aarambh/pull/2).
- An EFI partition mounted root-only (`dmask=0077` or `umask=0077`, common on
  Arch and archinstall) was not found, so `scripts/install.sh` could not start.
  It is now also recognized by its partition type, which needs no root.
  Contributed by [@DrArzter](https://github.com/DrArzter) in
  [#2](https://github.com/NarkAgni/aarambh/pull/2).
- CachyOS was taken for Debian's initramfs-tools because `cachyos-hooks` ships
  an `update-initramfs` wrapper around `mkinitcpio`: the config edit failed,
  kernel updates no longer refreshed the menu and the Plymouth `HOOKS` hint was
  skipped. initramfs-tools now also needs `/etc/initramfs-tools`.
  Contributed by [@DrArzter](https://github.com/DrArzter) in
  [#2](https://github.com/NarkAgni/aarambh/pull/2).

### Thanks

- [Dipak Yadav (@dipakmdhrm)](https://github.com/dipakmdhrm) for the timer,
  systemd-boot and write-combining fixes in 0.1.4
  ([#1](https://github.com/NarkAgni/aarambh/pull/1)).
- [@DrArzter](https://github.com/DrArzter) for the install, EFI partition and
  CachyOS fixes in this release
  ([#2](https://github.com/NarkAgni/aarambh/pull/2)).

## [0.1.4] - 2026-11-10

### Added

- Write-combining framebuffer: the menu maps the screen write-combining
  through the PAT while it runs and restores the firmware's memory types
  before starting any system. Animation is smooth on panels where a full
  frame took about 120 ms to show (2560x1600). On by default;
  `write_combining no` turns it off.
  Contributed by [Dipak Yadav (@dipakmdhrm)](https://github.com/dipakmdhrm) in
  [#1](https://github.com/NarkAgni/aarambh/pull/1).
- `\EFI\aarambh\wc.state` records the first attempt, so a boot that hung with
  write-combining turns it off on the next boot by itself.
- `scripts/install.sh` turns write-combining off inside virtual machines other
  than QEMU/KVM and on Apple or coreboot firmware. `aarambh-debug` shows
  `wc.state`.
- `docs/SECURE_BOOT.md`: installing with Secure Boot on, every MokManager
  screen with a screenshot, and every question about the password.
- README: all packages for building, Secure Boot, `make test` and
  `make qemu SECURE_BOOT=1` in the Requirements table.

### Fixed

- Countdown and animations ran far too slow on firmware whose `Stall()` waits
  longer than asked (30 times on an LG gram 16Z90Q). The timer is now
  calibrated from CPUID leaf 0x15 or a UEFI timer event, and frame pacing no
  longer uses `Stall()`.
  Contributed by [Dipak Yadav (@dipakmdhrm)](https://github.com/dipakmdhrm) in
  [#1](https://github.com/NarkAgni/aarambh/pull/1).
- The one-time kernel choice never reached systemd-boot: the
  `LoaderEntryOneShot` variable was written with a mistyped vendor GUID.
  Contributed by [Dipak Yadav (@dipakmdhrm)](https://github.com/dipakmdhrm) in
  [#1](https://github.com/NarkAgni/aarambh/pull/1).
- The first restart after `make install` showed the plain restart splash
  instead of the chosen theme's: reinstalling the Plymouth theme dropped the
  restart and shutdown frames Aarambh drew for it. They are now copied back
  right away.

## [0.1.3] - 2026-11-10

### Added

- Secure Boot support. Aarambh now starts through the distribution's own
  shim, which every PC trusts, and is signed with a Machine Owner Key (MOK).
  Everything is set up by `scripts/install.sh` and `hooks/setup.sh`:
  - An already enrolled key is reused (Ubuntu's shim-signed MOK, Fedora's
    akmods key, the DKMS key) unless it is marked for kernel modules only,
    which shim refuses for boot loaders. Otherwise a key is created in
    `/var/lib/aarambh/keys/` and enrolled once in MokManager with a random
    8-digit password (`sudo aarambh-sign --password` shows it again).
  - Aarambh asks shim to check every kernel and loader it starts, so kernels
    signed by the distribution or by the MOK key boot directly, and anything
    else is still refused.
  - Kernels copied for direct boot are signed with the OS's key, so every
    distribution on the disk boots directly, whichever shim starts Aarambh.
  - Shim and MokManager are copied next to Aarambh and kept current by
    `aarambh-sign` at every start and after apt and pacman updates.
  - Without shim, keys enrolled with sbctl are used.
- `.sbat` section in `aarambh.efi`, required by shim 15.3 and later.
- `aarambh-debug` checks the signatures, the boot entry, shim and the key
  enrollment.
- `make qemu SECURE_BOOT=1` (`tests/qemu/run.sh --secure-boot`): boots with
  Secure Boot on, Microsoft keys and a real shim.
- `make test` scenario for Ubuntu with Secure Boot on.

### Changed

- The Aarambh firmware boot entry is created after `setup.sh` and points at
  shim when Aarambh is signed; an existing entry with another path is
  replaced.
- Aarambh reads every image into memory before starting it.
- `uninstall.sh --purge` also removes Aarambh's own key (never a DKMS,
  akmods or shim-signed key) from the trusted keys.

## [0.1.2] - 2026-11-10

### Added

- Departures theme: an airport split-flap board. Every OS is a flight with its
  kernel, status and gate, and the letters flip when the selection changes.
- Gallery theme: a cover flow of wallpaper cards that turn away to the sides
  and reflect on a dark floor.
- `draw_image_perspective`, `draw_image_reflection` and `image_round_corners`
  for theme authors.

## [0.1.1] - 2026-11-09

### Added

- `aarambh-debug` (`make debug` from the source folder): writes one report
  with the platform, Secure Boot, firmware entries, ESP contents, kernel
  lists, GRUB, initramfs, Plymouth, graphics and the logs of this and the
  previous boot, with a summary of the problems it found.
- Supported hardware (x86_64 with 64-bit UEFI) stated at the top of the README;
  `make` and `scripts/install.sh` stop on any other machine.
- README troubleshooting for `aarambh: command not found` and a black screen
  after installing.

### Fixed

- `aarambh` is installed to `/usr/local/bin` as documented, instead of
  `/usr/local/sbin`, which is not in a normal user's `PATH` on Debian, openSUSE
  and others ("command not found").
- `scripts/install.sh --theme`, `--default` and `--list-themes`, and
  `make install THEME=... DEFAULT=...` and `make themes`, now work as
  documented. Before, `--theme` was passed on to `setup.sh`, which stopped
  before connecting the OS.
- `make themes`, `make test` and `make debug` no longer need gnu-efi.
- The gnu-efi error in `make` no longer suggests `apt` on every distribution.

## [0.1.0] - 2026-11-08

First public release.

### Added

- Graphical UEFI boot menu that starts Linux kernels directly from the EFI
  partition, with GRUB or the OS's own loader as a fallback.
- 11 themes: Accordion (default), Minimal, Glass Sidebar, Editorial,
  Editorial Dark, Cinema, Honeycomb, Constellation, Picker Wheel, Dial and HUD.
- Advanced options: pick any installed kernel or recovery entry for one boot.
- Boot options editor: press E to edit the kernel command line for one boot,
  like GRUB's `e`. Can be turned off with `edit no`.
- Plymouth boot splash that continues the current theme's launch animation,
  with no initramfs rebuild when the theme changes.
- Restart and shutdown splashes that follow the current theme.
- Autodetection of Windows and other EFI loaders on the EFI partition.
- Logos, wallpapers and Plymouth themes for 24 distributions, Windows and
  generic Linux.
- `aarambh` command to set the theme, default OS and timeout from any
  connected OS (`--theme`, `--default`, `--timeout`, `--themes`, `--list`).
- `scripts/install.sh` with `--theme`, `--default`, `--dry-run` and
  `--list-themes`; `make install THEME=... DEFAULT=...`.
- `hooks/setup.sh` for each Linux OS: kernel lists, kernel update hooks,
  Plymouth theme and boot service, for the Debian, Fedora/Red Hat, openSUSE,
  Arch, Gentoo and Void families. `hooks/uninstall.sh` reverts everything.
- Default entry chosen by OS id or menu title, so it survives entries changing
  order.
- QEMU test runner (`make qemu THEME=... DEFAULT=...`) and automated hook tests
  (`make test`).

[Unreleased]: https://github.com/NarkAgni/aarambh/compare/v0.1.7...HEAD
[0.1.7]: https://github.com/NarkAgni/aarambh/compare/v0.1.6...v0.1.7
[0.1.6]: https://github.com/NarkAgni/aarambh/compare/v0.1.5...v0.1.6
[0.1.5]: https://github.com/NarkAgni/aarambh/compare/v0.1.4...v0.1.5
[0.1.4]: https://github.com/NarkAgni/aarambh/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/NarkAgni/aarambh/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/NarkAgni/aarambh/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/NarkAgni/aarambh/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/NarkAgni/aarambh/releases/tag/v0.1.0