# Changelog

All notable changes to Aarambh are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/NarkAgni/aarambh/compare/v0.1.5...HEAD
[0.1.5]: https://github.com/NarkAgni/aarambh/compare/v0.1.4...v0.1.5
[0.1.4]: https://github.com/NarkAgni/aarambh/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/NarkAgni/aarambh/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/NarkAgni/aarambh/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/NarkAgni/aarambh/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/NarkAgni/aarambh/releases/tag/v0.1.0