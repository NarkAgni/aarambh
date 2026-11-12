# Aarambh

**Aarambh** (आरम्भ, "the beginning") is a graphical boot menu for Linux PCs.
It replaces the GRUB or systemd-boot menu with animated, full-screen panels and
hands over to a matching boot splash, so the screen never goes black between
the firmware logo and the login screen.

<p align="center">
<img src="docs/screenshots/accordion.jpg" alt="Aarambh with the Accordion theme" width="900">
</p>

- Works with any Linux distribution that boots in UEFI mode, with several of
  them side by side, and finds Windows on its own.
- 13 themes, switched with one command.
- **Your current boot loader is kept.** GRUB or systemd-boot stays installed
  as the fallback, and one command removes Aarambh again.

## Themes

13 built-in themes. Switch with `aarambh --theme <name>`; each has a matching
boot, restart and shutdown splash.

<table>
<tr>
<td align="center" width="33%">
<img src="docs/screenshots/accordion.jpg" alt="Accordion theme"><br>
<b>Accordion</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/cinema.jpg" alt="Cinema theme"><br>
<b>Cinema</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/hud.jpg" alt="HUD theme"><br>
<b>HUD</b>
</td>
</tr>
<tr>
<td align="center" width="33%">
<img src="docs/screenshots/glass-sidebar.jpg" alt="Glass Sidebar theme"><br>
<b>Glass Sidebar</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/editorial-dark.jpg" alt="Editorial Dark theme"><br>
<b>Editorial Dark</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/editorial.jpg" alt="Editorial theme"><br>
<b>Editorial</b>
</td>
</tr>
<tr>
<td align="center" width="33%">
<img src="docs/screenshots/honeycomb.jpg" alt="Honeycomb theme"><br>
<b>Honeycomb</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/constellation.jpg" alt="Constellation theme"><br>
<b>Constellation</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/picker-wheel.jpg" alt="Picker Wheel theme"><br>
<b>Picker Wheel</b>
</td>
</tr>
<tr>
<td align="center" width="33%">
<img src="docs/screenshots/dial.jpg" alt="Dial theme"><br>
<b>Dial</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/minimal.jpg" alt="Minimal theme"><br>
<b>Minimal</b>
</td>
<td align="center" width="33%">
<img src="docs/screenshots/departures.jpg" alt="Departures theme"><br>
<b>Departures</b>
</td>
</tr>
<tr>
<td align="center" width="33%">
<img src="docs/screenshots/gallery.jpg" alt="Gallery theme"><br>
<b>Gallery</b>
</td>
</tr>
</table>

Each theme with a larger screenshot and its keys is listed under **Themes**
in [Using Aarambh](#using-aarambh).

**On this page**

| I want to...                         | Go to |
|--------------------------------------|-------|
| See every theme                      | [Themes](#themes) |
| Install Aarambh                      | [Install](#install), 6 steps |
| Change theme, default OS or timeout  | [Everyday use](#everyday-use) |
| Update to a new version              | [Update](#update) |
| Remove it                            | [Uninstall](#uninstall) |
| Get back into my system              | [If something goes wrong](#if-something-goes-wrong) |
| Know exactly what it changes         | [Reference](#reference) |

---

## Install

Do the steps in order. Every command is typed in a terminal, on the Linux you
use every day. The whole thing takes about five minutes.

### Step 1: Check that your PC is supported

```sh
uname -m                                  # must print: x86_64
cat /sys/firmware/efi/fw_platform_size    # must print: 64
```

| Result                                   | Meaning |
|------------------------------------------|---------|
| `x86_64` and `64`                        | Supported. Go to step 2. |
| `aarch64`, `i686`, `riscv64`             | Not supported (ARM, Raspberry Pi, Apple Silicon, 32-bit PCs, RISC-V). |
| The second command says *No such file*   | The PC booted in legacy BIOS / CSM mode. Not supported. |
| `x86_64` and `32`                        | 32-bit UEFI firmware (some Atom tablets). Not supported. |

Secure Boot can stay **on**. The installer checks all of this again and stops
before it changes anything.

### Step 2: Install the tools needed to build it

Run the **one** line for your distribution:

| Distribution                         | Command |
|--------------------------------------|---------|
| Ubuntu, Debian, Linux Mint, Pop!_OS, Zorin, Kali | `sudo apt install git build-essential gnu-efi curl efibootmgr` |
| Fedora, RHEL, Rocky, CentOS Stream   | `sudo dnf install git gcc make gnu-efi-devel curl efibootmgr` |
| openSUSE                             | `sudo zypper install git gcc make gnu-efi curl efibootmgr` |
| Arch, Manjaro, EndeavourOS, CachyOS  | `sudo pacman -S --needed git base-devel gnu-efi curl efibootmgr` |

Nothing else is needed. With Secure Boot on, the installer adds the signing
tools itself.

### Step 3: Download and install

```sh
git clone https://github.com/NarkAgni/aarambh.git
cd aarambh
make install
```

It asks for your password once (`sudo`), because it writes to the EFI
partition. When it has worked, the last lines look like this:

```
aarambh: done: Ubuntu is connected to Aarambh
    Linux 6.8.0-45-generic   [direct boot]
    Linux 6.8.0-45-generic (recovery)
    Linux 6.8.0-40-generic
aarambh: EFI partition free: 412M
aarambh: change theme or default OS with: aarambh --theme <n> --default <os>
aarambh: if anything goes wrong, run aarambh-debug and attach the report to an issue
aarambh: installed to /boot/efi/EFI/aarambh
```

<details>
<summary>Optional: see every change first, or pick a theme while installing</summary>

<br>

```sh
./scripts/install.sh --dry-run            # print every change, change nothing
make themes                               # list the 13 themes
make install THEME=cinema DEFAULT=fedora  # install with a theme and a default OS
make qemu THEME=hud                       # try a theme in a virtual machine first
```

`make qemu` needs the extra packages in the *Requirements* table under
[Development](#development).

</details>

If it stops with an error instead, look the message up under
[Troubleshooting](#troubleshooting). Nothing is half installed: run
`make install` again after the fix.

### Step 4: Secure Boot only: note the 8-digit password

Look at the end of the installer's output.

- **No password is shown:** nothing to do. Go to step 5.
- **These lines are shown:** write the 8 digits down, you need them once in
  step 5.

  ```
  aarambh: Secure Boot: one step is left, only once, at the next restart.
  aarambh:   A blue 'Perform MOK management' screen appears. Choose:
  aarambh:   Enroll MOK -> Continue -> Yes, type the password: 48213907 -> Reboot
  ```

  Forgot it? `sudo aarambh-sign --password` shows it again.

### Step 5: Restart

```sh
sudo reboot
```

| What you see                          | What to do |
|---------------------------------------|------------|
| The Aarambh menu                      | Done. Press **Enter** to start, or wait for the countdown. |
| A blue **Perform MOK management** screen (only if step 4 showed a password) | Press a key, then choose **Enroll MOK → Continue → Yes**, type the 8 digits, choose **Reboot**. The Aarambh menu comes next. Every screen is shown in [docs/SECURE_BOOT.md](docs/SECURE_BOOT.md). |
| Your old boot menu, or straight into Linux | Aarambh was not started. Nothing is broken. See [If something goes wrong](#if-something-goes-wrong). |
| A black screen for more than a minute | See [If something goes wrong](#if-something-goes-wrong). |

Missed the blue screen? Nothing breaks: the PC starts your OS as before and
the blue screen comes back at the next restart.

### Step 6: Connect your other Linux systems (skip with only one Linux)

Windows needs nothing: it is already in the menu.

Every **other Linux** on the same disk is in the menu too, but it only gets
its kernel list, direct boot and matching splash after you connect it. Pick it
in the Aarambh menu, log in, and run this once:

```sh
sudo bash /boot/efi/EFI/aarambh/hooks/setup.sh
```

If it says *No such file or directory*, that OS mounts the EFI partition
somewhere else. Find it and use that path instead:

```sh
findmnt -t vfat          # prints /boot/efi, /efi or /boot
sudo bash /efi/EFI/aarambh/hooks/setup.sh
```

That is all. Kernel updates are picked up automatically from now on.

---

## Everyday use

### Keys in the menu

| Key                 | Action                                  |
|---------------------|-----------------------------------------|
| Left / Right        | Switch operating system                 |
| Enter               | Start it                                |
| Down, then Enter    | Advanced options: older kernels, recovery mode |
| Esc                 | Go back                                 |
| E                   | Edit the boot options for one boot      |
| Any key             | Stops the countdown                     |

Some themes list the systems vertically; then Up / Down switch and Right opens
the kernel list. The keys that work are always shown at the bottom of the
screen.

### Change theme, default OS or countdown

Run these from any connected Linux, as your normal user (it asks for the
password itself). The change applies from the next boot, for every OS.

```sh
aarambh                       # show the current settings
aarambh --themes              # list the themes, * marks the current one
aarambh --theme cinema        # switch theme
aarambh --list                # list the menu entries, * marks the default
aarambh --default fedora      # start Fedora by default
aarambh --timeout 3           # 3 second countdown
aarambh --timeout 0           # no countdown: the menu waits for a key (0 to 999 seconds)
```

The countdown only ever starts an operating system. With Restart, Power Off or
Firmware selected, the menu waits for you.

**Want a faster boot?** The default countdown is 10 seconds.
`aarambh --timeout 2` saves 8 of them and still leaves time to press a key.

---

## Update

```sh
cd aarambh
git pull
make install
```

Then run the `setup.sh` line from [step 6](#step-6-connect-your-other-linux-systems-skip-with-only-one-linux)
once on every other Linux, so its part is updated too. Your settings and menu
entries are kept. Windows needs nothing.

## Uninstall

On each Linux that was connected, except the last one:

```sh
sudo bash /boot/efi/EFI/aarambh/hooks/uninstall.sh
```

On the last one, add `--purge`. It also deletes Aarambh from the EFI partition
and removes its firmware boot entry:

```sh
sudo bash /boot/efi/EFI/aarambh/hooks/uninstall.sh --purge
```

Every file Aarambh changed is restored from its backup, and the PC boots the
way it did before. Add `--dry-run` to either command to see what it would do.

## If something goes wrong

Your old boot loader is never removed, so there is always a way in.

**1. Get back into Linux.**

- Black screen? Wait a minute first. With an encrypted disk it may be the
  password prompt: type the password and press **Enter**.
- Otherwise switch the PC off (hold the power button), switch it on and open
  the **firmware boot menu**: tap **F12** (often **F8**, **F11** or **Esc**,
  depending on the brand) right after power on.
- Choose your distribution's own entry: `ubuntu`, `fedora`, `GRUB`,
  `Linux Boot Manager`. It boots exactly as it did before Aarambh.

**2. Find out what happened.**

```sh
aarambh-debug       # or, in the source folder: make debug
```

It changes nothing and writes `~/aarambh-debug-<date>.txt`. The first lines
are a summary; every line marked `[PROBLEM]` says what is wrong and how to fix
it.

**3. Fix it, or ask.** Look the message up under
[Troubleshooting](#troubleshooting). If that does not help, attach the report
to a [new issue](https://github.com/NarkAgni/aarambh/issues/new). To go back
to the old boot completely, see [Uninstall](#uninstall).

Messages the menu itself can show:

| Message in the menu                    | Meaning |
|----------------------------------------|---------|
| Could not start this system            | The loader of that entry failed. You are back in the menu; pick another entry. |
| Blocked by Secure Boot                 | That kernel or loader is not signed with a trusted key. Run `setup.sh` on that OS. |
| Could not save the kernel choice, nothing was started | The kernel you picked under *Advanced options* could not be handed to the OS's boot loader (EFI partition full or read-only). Nothing was started, so no other kernel booted in its place. |
| No bootable system found               | No boot loader was found on the EFI partition. The menu waits; it does not restart by itself. Start your OS from the firmware boot menu and run `aarambh-debug`. |

---

# Reference

Everything below is detail: how Aarambh works and every change it makes to
your machine. Click a line to open it.

## What Aarambh is

<details>
<summary><b>What is Aarambh, in one paragraph?</b></summary>

<br>

Aarambh is a single UEFI application (`aarambh.efi`) that your firmware starts
instead of GRUB or systemd-boot. It draws a menu with one panel per operating
system plus Firmware, Restart and Power Off. When you pick an OS it either
starts the Linux kernel directly, or hands over to that OS's own loader (shim,
GRUB, systemd-boot, Windows Boot Manager). It does not replace or uninstall
any boot loader or kernel.

</details>

<details>
<summary><b>Features</b></summary>

<br>

- Accordion layout: the selected OS expands, the others collapse into side panels
- Works with every UEFI Linux distribution, with GRUB, GRUB + BLS or systemd-boot
- Finds Windows and other EFI loaders on the disk on its own
- Advanced options list with every installed kernel and recovery mode
- Direct Linux boot, with the OS's own boot loader as the fallback
- One-time kernel choice: pick an older kernel for one boot only
- Firmware setup, Restart and Power Off entries
- Matching Plymouth splash for a boot with no black screen
- Every change is recorded and can be undone with `hooks/uninstall.sh`
- 13 built-in themes, each with its own colors, switchable with one config line
- Theme API: new looks are drawn on top of a shared core, widgets and theme kit

</details>

<details>
<summary><b>How a boot works, step by step</b></summary>

<br>

```
Firmware (UEFI)
   │  BootOrder points to "Aarambh"
   ▼
aarambh.efi ── reads \EFI\aarambh\aarambh.conf, fonts, images, <os>.kernels
   │            and lists every other loader it finds under \EFI\
   │
   │  you choose an entry (or the countdown ends)
   ▼
┌───────────────────────────────┬──────────────────────────────────────────┐
│ Direct boot                   │ Fallback                                 │
│ (kernel available on the ESP) │ (no direct copy, or Windows / other OS)  │
├───────────────────────────────┼──────────────────────────────────────────┤
│ Loads the kernel from the ESP │ Remembers the chosen kernel:             │
│ Joins the initrd parts        │   GRUB: writes next-<os>.cfg             │
│ (microcode + initramfs) and   │   systemd-boot: sets LoaderEntryOneShot  │
│ serves them through the       │ Starts the OS's own loader               │
│ LINUX_INITRD_MEDIA protocol   │   (shim, GRUB, systemd-boot, Windows)    │
│ Passes the kernel cmdline     │   GRUB/systemd-boot boot the chosen      │
│ Starts the kernel EFI stub    │   kernel without showing a menu          │
└───────────────┬───────────────┴─────────────────────┬────────────────────┘
                ▼                                     ▼
         Plymouth "aarambh" theme (same background, logo and dots)
                ▼
         Login screen
                │
                └─ aarambh-clear.service empties next-<os>.cfg, so the next
                   boot uses the default kernel again
```

Which kernels can start directly:

| Where the kernels live                         | Direct boot              |
|------------------------------------------------|--------------------------|
| On the EFI partition (systemd-boot, ESP=/boot) | Every kernel, in place   |
| On another partition (most GRUB installs)      | The newest kernel, copied to the ESP |

A kernel is only started directly when everything it needs is known to be
complete. It goes through the OS's own loader instead when:

- its boot entry names an initramfs or microcode file that does not exist
- its entry uses GRUB's `$kernelopts` and `grubenv` does not set it
- its command line is longer than 2047 characters (the limit of an x86
  kernel) or it has more than four initramfs parts; nothing is ever cut short

Firmware, Restart and Power Off call the UEFI runtime services directly.
Firmware sets the `OsIndications` variable so the machine reboots into the
firmware setup screen.

</details>

<details>
<summary><b>Why the screen never goes black</b></summary>

<br>

A normal boot shows several different screens: firmware logo, boot loader
menu, a black gap while the kernel loads, then the Plymouth splash. Aarambh
removes the gaps:

- **GRUB is hidden, not removed.** Its timeout is set to `0` and its menu is
  hidden, so it passes through instantly when it is used at all.
- **Kernels skip the boot loader.** Aarambh starts them directly from the ESP.
- **The splash continues the menu, in every theme.** Just before a direct
  boot, Aarambh renders the last frame of the current theme's launch splash
  (without its dots) and appends it to the initrd as a small cpio archive.
  Linux unpacks it over the installed `aarambh` Plymouth theme, so Plymouth
  shows exactly that frame and animates the dots where Aarambh left them.
  Changing the theme needs no initramfs rebuild and works for every OS.
- **Plymouth starts immediately.** `ShowDelay=0` is set. On distributions using
  initramfs-tools (Debian, Ubuntu, Mint, ...) with Intel graphics, the `i915`
  driver is added to the initramfs so the graphics mode is ready early.

</details>

---

## Supported systems

<details>
<summary><b>Distributions and what is detected</b></summary>

<br>

`hooks/setup.sh` detects everything from the running system; nothing is
hard-coded per distribution.

| Detected                | How                                                        |
|-------------------------|------------------------------------------------------------|
| Distribution            | `ID`, `ID_LIKE` and `NAME` from `/etc/os-release`          |
| EFI partition           | vfat mounted at `/boot/efi`, `/efi` or `/boot`, or `bootctl --print-esp-path` |
| Boot loader             | GRUB (`/etc/default/grub` + `update-grub`/`grub-mkconfig`/`grub2-mkconfig`), otherwise systemd-boot |
| Kernel list             | BLS entries (Fedora family, systemd-boot) or kernel files in `/boot` |
| GRUB menu ids           | Read from the generated `grub.cfg`, so they match your distribution exactly |
| Microcode               | `intel-ucode.img`, `amd-ucode.img` and the other images GRUB knows, joined in front of the initramfs |
| Initramfs generator     | initramfs-tools, dracut, mkinitcpio or booster             |
| Kernel update hooks     | Whatever this system runs: initramfs-tools, kernel-install, mkinitcpio, `/etc/kernel/postinst.d`, Void's `kernel.d`, GRUB |
| OS loader on the ESP    | The `\EFI\<dir>\` whose `grub.cfg` points at this system, then `\EFI\<ID>\`, then `--loader` |

| Family               | Examples                                         | Status |
|----------------------|--------------------------------------------------|--------|
| Debian               | Debian, Ubuntu, Linux Mint, Pop!_OS, elementary, Zorin, Kali, deepin | Supported |
| Fedora / Red Hat     | Fedora, RHEL, CentOS Stream, Rocky               | Supported |
| openSUSE             | Tumbleweed, Leap, Slowroll                       | Supported |
| Arch                 | Arch, Manjaro, EndeavourOS, Garuda, CachyOS, Artix | Supported (GRUB or systemd-boot) |
| Independent          | Gentoo, Void                                     | Supported |
| NixOS                | NixOS                                            | Menu entry through autodetect only; `setup.sh` does not run on NixOS's read-only `/etc` |
| Windows              | Windows 10, 11                                   | Menu entry through autodetect |

Ubuntu and Fedora are tested on real hardware. Every family above is covered
by the automated tests in `tests/hooks/` (`make test`), which run `setup.sh`
and `uninstall.sh` against simulated installations. If something does not
work on your distribution, please open an issue with the report from
`aarambh-debug` (or `make debug`).

</details>

<details>
<summary><b>Hardware and firmware</b></summary>

<br>

| Machine                                        | Status        |
|------------------------------------------------|---------------|
| 64-bit x86 PC (`x86_64`, Intel or AMD), 64-bit UEFI | Supported |
| ARM64 (`aarch64`): Raspberry Pi, Apple Silicon, Snapdragon, ARM servers | Not supported |
| 32-bit x86 (`i686`)                            | Not supported |
| 64-bit CPU with 32-bit UEFI firmware           | Not supported |
| RISC-V                                         | Not supported |
| Legacy BIOS / CSM boot                         | Not supported |
| Secure Boot on                                 | Supported, through the distribution's shim (or sbctl keys) |

`aarambh.efi` is built for `x86_64` UEFI only (`Makefile`: `ARCH := x86_64`),
and the loaders it looks for (`shimx64.efi`, `grubx64.efi`,
`systemd-bootx64.efi`) are the x86_64 ones. `make` and `scripts/install.sh`
stop with an error on any other machine.

</details>

<details>
<summary><b>Distribution ids for logos</b></summary>

<br>

Each OS uses `logos/<id>.png`, `backgrounds/<id>.jpg` and `plymouth/<id>/`,
where `<id>` is the `ID=` value from `/etc/os-release`. Check yours with:

```sh
grep -E '^(ID|ID_LIKE)=' /etc/os-release
```

Every OS below ships with its own logo (from `assets/custom/`), wallpaper and
Plymouth theme, named and colored in `assets/distros.conf`:

| id | Distribution | id | Distribution |
|----|--------------|----|--------------|
| `ubuntu` | Ubuntu | `opensuse-tumbleweed` | openSUSE Tumbleweed |
| `debian` | Debian | `opensuse-leap` | openSUSE Leap |
| `linuxmint` | Linux Mint | `opensuse-slowroll` | openSUSE Slowroll |
| `pop` | Pop!_OS | `arch` | Arch Linux |
| `elementary` | elementary OS | `manjaro` | Manjaro |
| `zorin` | Zorin OS | `endeavouros` | EndeavourOS |
| `kali` | Kali Linux | `garuda` | Garuda Linux |
| `deepin` | deepin | `cachyos` | CachyOS |
| `fedora` | Fedora | `artix` | Artix Linux |
| `rhel` | Red Hat Enterprise Linux | `gentoo` | Gentoo |
| `centos` | CentOS Stream | `void` | Void Linux |
| `rocky` | Rocky Linux | `nixos` | NixOS |
| `windows` | Windows (autodetected) | `linux` | Any other Linux found through GRUB |

Any other distribution still works: add `assets/custom/<id>-logo.svg` and a line
in `distros.conf` (see **Custom logos** under *Using Aarambh*).

Autodetected `systemd-boot` and `refind` entries have no logo of their own.

Kubuntu, Xubuntu and Lubuntu report `ID=ubuntu` and use the Ubuntu look.
An OS without a logo is shown with the first letter of its name and its accent
color, so a missing logo never breaks the menu.

</details>

---

## What the installer changes

<details>
<summary><b>Why does Aarambh need root (sudo)?</b></summary>

<br>

Every step that needs root, and why:

| Action                                         | Why root is needed                               |
|------------------------------------------------|--------------------------------------------------|
| Copy files to the EFI partition                | It is mounted read-only for normal users         |
| Create the "Aarambh" boot entry (`efibootmgr`) | Writes firmware NVRAM variables                  |
| Install scripts to `/usr/local` and `/etc`     | System directories                               |
| Edit `/etc/default/grub`, regenerate `grub.cfg`| System boot loader configuration                 |
| Install the Plymouth theme, rebuild initramfs  | System boot files                                |
| Enable `aarambh-clear.service`, `aarambh-shutdown-splash.service` | System-wide systemd units          |
| Sign Aarambh and kernels, ask shim to enroll the key (`mokutil`) | The signing key is root-only; enrollment writes firmware variables |

`make` (the build) and `make test` never need root. Only `scripts/install.sh`,
`hooks/setup.sh` and `hooks/uninstall.sh` use `sudo`.

</details>

<details>
<summary><b>See every change before it happens (<code>--dry-run</code>)</b></summary>

<br>

```sh
./scripts/install.sh --dry-run
sudo bash /boot/efi/EFI/aarambh/hooks/setup.sh --dry-run
```

Both print what they detected and every command and file write they would
make, without changing anything.

</details>

<details>
<summary><b>What <code>scripts/install.sh</code> does</b></summary>

<br>

Run once, from the OS you install from:

1. Checks the system booted in UEFI mode and finds the EFI partition.
2. Builds `build/aarambh.efi` with `make`.
3. Removes files left by older Aarambh versions from `\EFI\aarambh\`. Nothing
   outside that folder is touched.
4. Copies `aarambh.efi`, `distros.conf`, `fonts/`, `logos/`, `backgrounds/`,
   `plymouth/` and `hooks/` into `\EFI\aarambh\`. An existing `aarambh.conf`
   is kept, because it holds the entries of your other systems; use
   `--reset-config` to replace it with the default.
5. Runs `hooks/setup.sh` for the current OS, which also signs Aarambh for
   Secure Boot (see **Secure Boot** under *Safety and recovery*).
6. Creates a firmware boot entry named **Aarambh** with `efibootmgr`. It
   starts shim when Aarambh is signed, `aarambh.efi` otherwise. An existing
   entry with the same path is kept, one with another path is replaced.
   `efibootmgr` puts a new entry first in `BootOrder`. Your existing boot
   entries stay.

7. With `--theme` or `--default`, sets them in `aarambh.conf` through the
   `aarambh` command.

Options: `--dry-run`, `--reset-config`, `--theme <name>`, `--default <os>`,
plus any `setup.sh` option. `make install THEME=<name> DEFAULT=<os>` passes the
last two through.

</details>

<details>
<summary><b>What <code>hooks/setup.sh</code> does</b></summary>

<br>

Run once on **each** Linux installation that Aarambh boots.

1. Detects the distribution, EFI partition, boot loader, initramfs generator
   and the OS loader on the ESP, and prints them.
2. Writes `/etc/aarambh.env` with those results.
3. Installs the helper scripts (see the next section).
4. Installs the refresh trigger into every kernel hook directory this system
   uses.
5. Installs and enables `aarambh-clear.service` and
   `aarambh-shutdown-splash.service` (systemd only).
6. Adds or updates this OS's line in `aarambh.conf`.
7. GRUB only: makes GRUB silent (see **GRUB changes** below).
8. initramfs-tools only: `MODULES=dep` and, with Intel graphics, `i915`.
9. Installs the Plymouth theme for this distribution, if Plymouth is
   installed and a theme exists for its id.
10. Rebuilds the initramfs (only when steps 8 or 9 changed something) and
    regenerates `grub.cfg`.
11. Runs `aarambh-kernels` once and prints the kernel list.
12. Secure Boot: picks the signing key, copies shim and MokManager next to
    Aarambh, signs `aarambh.efi` and the direct-boot kernel and, with Secure
    Boot on and an untrusted key, asks shim to enroll it and prints the
    password.

Every file it creates is listed in `/etc/aarambh/manifest`. Every system file
it edits is first copied to `<file>.aarambh-backup`.

Options:

| Option                          | Effect                                         |
|---------------------------------|------------------------------------------------|
| `--dry-run`                     | Print the changes, change nothing              |
| `--no-plymouth`                 | Leave the boot splash alone                    |
| `--loader '\EFI\<dir>\<file>.efi'` | Use this loader when detection cannot find it |

</details>

<details>
<summary><b>Scripts, hooks and the systemd service it installs</b></summary>

<br>

| Installed file                                  | Installed on                  | Runs when                                | What it does |
|-------------------------------------------------|-------------------------------|------------------------------------------|--------------|
| `/usr/local/sbin/aarambh-kernels`               | every system                  | Called by the triggers below             | Writes `<os>.kernels` on the ESP, keeps the direct-boot copy current, updates `utc_offset` |
| `/usr/local/lib/aarambh/*.sh`                   | every system                  | Sourced by `aarambh-kernels`             | Kernel discovery (BLS and `/boot` scan), cmdline, ESP copy |
| `/etc/grub.d/01_aarambh`                        | GRUB systems                  | Every GRUB config regeneration           | Refreshes the list and adds the snippet that applies `next-<os>.cfg` |
| `/etc/initramfs/post-update.d/zz-aarambh`       | initramfs-tools               | After every initramfs update             | Refresh trigger |
| `/etc/kernel/install.d/99-aarambh.install`      | systems with `kernel-install` | After a kernel is added or removed       | Refresh trigger |
| `/etc/initcpio/post/aarambh`                    | mkinitcpio                    | After every initramfs build              | Refresh trigger |
| `/etc/kernel/postinst.d/zzz-aarambh`, `postrm.d/zzz-aarambh` | systems with these folders | After kernel install/removal, after GRUB updates | Refresh trigger |
| `/etc/kernel.d/post-install/99-aarambh`         | Void                          | After a kernel install                   | Refresh trigger |
| `/usr/local/bin/aarambh`                        | every system                  | When you run it                          | Sets theme, default OS and timeout in `aarambh.conf` |
| `/usr/local/bin/aarambh-debug`                  | every system                  | When you run it                          | Writes a debug report to your home folder, changes nothing |
| `/usr/local/sbin/aarambh-clear`                 | systemd systems               | Called by the service below              | Empties `next-<os>.cfg`, runs `aarambh-sign` |
| `/usr/local/sbin/aarambh-sign`                  | every system                  | At every start, after apt and pacman updates, when you run it | Copies a newer shim next to Aarambh, asks shim to enroll the key once Secure Boot is on; `--password` shows the MokManager password |
| `/etc/apt/apt.conf.d/99aarambh`                 | apt systems booting through shim | After every apt run                   | Runs `aarambh-sign` |
| `/etc/pacman.d/hooks/aarambh-shim.hook`         | pacman systems booting through shim | After a shim package update        | Runs `aarambh-sign` |
| `/var/lib/aarambh/keys/`                        | systems without a reusable key | Read when signing                       | Aarambh's own signing key and the MokManager password, root-only |
| `/etc/systemd/system/aarambh-clear.service`     | systemd systems               | Once per boot                            | Makes a one-time kernel choice last for one boot only; copies the themed restart and shutdown frames into the Plymouth theme |
| `/etc/systemd/system/aarambh-shutdown-splash.service` | systemd systems that return to the initramfs at shutdown (dracut, mkinitcpio) | At restart and shutdown | Gives the shutdown initramfs the current Aarambh splash, so Plymouth keeps the theme until power off |
| `/etc/aarambh.env`                              | every system                  | Read by all scripts above                | Detected OS id, ESP, boot loader, signing key and shim |
| `/etc/aarambh/manifest`                         | every system                  | Read by `uninstall.sh`                   | List of everything to undo |

The refresh trigger is `hooks/triggers/aarambh-refresh`. It always ends with
`exit 0`, so a problem in Aarambh never breaks a kernel update. Its warnings
and errors go to the system log; read them with `journalctl -t aarambh`.
Kernel updates need no action from you.

Two triggers that fire at the same time wait for each other, so the kernel
list and the kernel copy are only ever written by one of them. Every command
that changes `aarambh.conf` (`aarambh`, `setup.sh`, `uninstall.sh` and the
clock update in `aarambh-kernels`) takes the same lock,
`/run/lock/aarambh-config.lock`, so two changes made at the same moment are
both kept.

Without systemd the one-time choice is still reset: Aarambh empties
`next-<os>.cfg` on the next boot it starts with the default kernel.

</details>

<details>
<summary><b>GRUB changes</b></summary>

<br>

GRUB stays installed and keeps working. Aarambh only makes it silent. The
original is saved as `/etc/default/grub.aarambh-backup` before the first
change.

All GRUB systems:

| Setting        | New value | Reason                              |
|----------------|-----------|-------------------------------------|
| `GRUB_TIMEOUT` | `0`       | GRUB passes through without waiting |

GRUB systems without BLS (Debian, Ubuntu, Arch, openSUSE, Gentoo, ...):

| Setting                   | New value     | Reason                                         |
|---------------------------|---------------|------------------------------------------------|
| `GRUB_TIMEOUT_STYLE`      | `hidden`      | No menu flash                                  |
| `GRUB_TERMINAL_OUTPUT`    | `console`     | No GRUB graphics mode switch                   |
| `GRUB_GFXPAYLOAD_LINUX`   | `keep`        | Kernel keeps the firmware framebuffer          |
| `GRUB_DISABLE_OS_PROBER`  | `true`        | Aarambh already lists every OS                 |
| `GRUB_RECORDFAIL_TIMEOUT` | `0`           | No 30-second menu after an unclean shutdown    |
| `GRUB_THEME`, `GRUB_GFXMODE`, `GRUB_SAVEDEFAULT` | commented out | Would show or switch screens |

To reach the GRUB menu after installing, hold **Esc** or **Shift** while GRUB
starts, or pick the distribution's own entry from your firmware boot menu.

systemd-boot systems: `loader.conf` is not changed. Aarambh sets
`LoaderEntryOneShot` when it hands over, so systemd-boot boots the chosen entry.

</details>

<details>
<summary><b>What Aarambh stores on the EFI partition</b></summary>

<br>

```
\EFI\aarambh\
├── aarambh.efi                  The boot menu
├── aarambh.conf                 Menu configuration (one line per OS)
├── distros.conf                 Names and colors of known distributions
├── fonts\  logos\  backgrounds\ Menu assets
├── plymouth\<id>\               Boot splash themes
├── hooks\                       Copy of the repo's hooks/ for the other systems
├── linux\<id>\vmlinuz-<n>       Copy of the newest kernel (direct boot)
├── linux\<id>\initrd-<n>.img    Its initramfs, with microcode in front
├── <id>.kernels                 Kernel list written by aarambh-kernels
└── next-<id>.cfg                One-time GRUB kernel choice, usually empty
```

Nothing is written outside `\EFI\aarambh\`. The direct-boot copy is the
largest item. Before copying, `aarambh-kernels` checks that at least 8 MB of
the ESP stays free. When the new pair only fits once the old one is gone, it
first publishes a list that starts the new kernel through the OS's own
loader, then deletes the old pair, then copies the new one and publishes the
list again. If the kernel does not fit even then, it prints a warning and
that OS keeps booting through its own loader. Systems whose kernels already live
on the ESP (systemd-boot, ESP mounted at `/boot`) need no copy at all.

The kernel and its initramfs are copied as a pair with the same `<n>` in
their names (12 characters made from the content of both files), and
`<id>.kernels` names the pair it boots. After a kernel update the new pair is
written next to the old one, the list is replaced in one step, and only then
is the old pair deleted. An update that is interrupted or fails halfway
therefore never leaves a kernel paired with the initramfs of another one, and
never leaves a half-written list: the menu keeps the last complete one, and
every file that list names. A kernel scan that fails partway (an unreadable
`/boot`, a failing helper) is an error too, never an empty list.

Before a copy is used again it is checked: the initramfs and an unsigned
kernel byte for byte against the source, a signed kernel by its signature
with this OS's key. A copy that was damaged on the ESP is written again.

</details>

<details>
<summary><b>What <code>aarambh.efi</code> can and cannot do at boot</b></summary>

<br>

At boot time Aarambh:

- reads files inside `\EFI\aarambh\`, kernel files named in `<id>.kernels`,
  and the folder names under `\EFI\` (for autodetect)
- writes only `next-<id>.cfg` (and only when the kernel choice changed), the
  restart and shutdown splash images under `shutdown\` and `wc.state` on the
  ESP
- maps the framebuffer write-combining while the menu runs, and puts the
  firmware's memory types back before it starts any system
- writes firmware variables only when you act: `OsIndications` when you choose
  **Firmware**, `LoaderEntryOneShot` when it hands over to systemd-boot
- has no network code and sends nothing anywhere

It cannot read your Linux partitions. Kernels for direct boot are either
already on the ESP or copies made by the hook while Linux was running.

</details>

---

## Safety and recovery

<details>
<summary><b>Secure Boot</b></summary>

<br>

Aarambh works with Secure Boot on. Nothing has to be set up by hand. For the
one-time key enrollment, screen by screen with screenshots, and every question
about the password, see **[docs/SECURE_BOOT.md](docs/SECURE_BOOT.md)**.

```
Firmware (trusts Microsoft's keys)
  → shimx64.efi          your distribution's shim, signed by Microsoft
  → grubx64.efi          Aarambh, signed with your Machine Owner Key (MOK)
  → menu
  → Linux: the kernel is checked by shim, then started directly
  → Windows: bootmgfw.efi, signed by Microsoft
```

**How it is set up.** `setup.sh` finds a shim on the EFI partition (this OS's
first, then any other distribution's) and copies it, with MokManager, into
`\EFI\aarambh\`. It signs `aarambh.efi` and saves it as the file shim starts
(`grubx64.efi`, or `grub.efi` with openSUSE's shim). `install.sh` then points
the **Aarambh** boot entry at that shim.

**Which key.** A key your PC already trusts is reused when shim accepts it
for boot loaders. Shim refuses any key marked for kernel modules only
(extended key usage `1.3.6.1.4.1.2312.16.1.2`), and the driver keys Ubuntu and
DKMS create are marked that way, so those are skipped:

| Key                                             | Comes from | Used for Aarambh |
|-------------------------------------------------|------------|------------------|
| `/var/lib/shim-signed/mok/MOK.der`              | Ubuntu, Debian (made for DKMS drivers such as NVIDIA) | Only if not marked for modules |
| `/etc/pki/akmods/certs/public_key.der`          | Fedora akmods | Only if not marked for modules |
| `/var/lib/dkms/mok.pub`                         | DKMS 3 | Only if not marked for modules |
| `/var/lib/aarambh/keys/aarambh.der`             | Made by Aarambh when none of the above can be used | Always |

**The one-time step.** When the key is not trusted yet, `setup.sh` asks shim
to enroll it and prints a random 8-digit password. At the next restart a blue
**Perform MOK management** screen appears: **Enroll MOK → Continue → Yes**,
type the password, **Reboot**. The password is only digits because MokManager
reads every keyboard as a US layout, and the digit keys are in the same place
on all layouts. It is kept root-only in `/var/lib/aarambh/keys/mok-password`;
`sudo aarambh-sign --password` shows it again. If you miss the screen, nothing
breaks: shim cannot start Aarambh, the firmware moves on to your OS's own boot
entry, and the screen comes back at the next restart.

**Kernels.** Aarambh asks shim to check every kernel and loader it starts, the
same check GRUB uses. Distribution kernels pass, kernels signed with your MOK
key pass, anything else is refused and Aarambh shows **"Blocked by Secure
Boot"** and starts your OS's own boot loader instead. The kernel copy made for
direct boot is signed with the OS's key, so every Linux on the disk boots
directly, whichever distribution's shim started Aarambh. Kernel updates are
signed automatically.

**Turning Secure Boot on later.** Aarambh is signed even with Secure Boot off.
If you turn it on and the key is not enrolled yet, the PC starts your OS's own
boot loader once, `aarambh-sign` notices at that start and asks shim to enroll
the key, and the blue screen appears at the following restart. Read the
password with `sudo aarambh-sign --password` before you restart.

**Shim updates.** `aarambh-sign` copies a newer shim and MokManager next to
Aarambh after apt and pacman updates and at every start, so shim updates and
SBAT revocations reach Aarambh's copy too.

**sbctl.** Without shim, keys you enrolled with `sbctl` are used, and the
firmware checks Aarambh and the kernels directly.

**Removing the key.** `uninstall.sh --purge` asks shim to remove Aarambh's own
key (never a DKMS, akmods or shim-signed key) at the next restart: **Delete MOK
→ Continue → Yes**, with the password it prints.

</details>

<details>
<summary><b>If Aarambh does not start or shows an error</b></summary>

<br>

Your old boot path is never removed:

1. Open your firmware boot menu (often **F12**, **F8**, **F11** or **Esc** at
   power on, depending on the vendor).
2. Choose your distribution's own entry (`ubuntu`, `fedora`, `GRUB`,
   `Linux Boot Manager`, ...). That starts its loader exactly as before.
3. Once in Linux, fix or remove Aarambh (see below).

If a system fails to start from the menu, Aarambh shows **"Could not start
this system"** and returns to the menu instead of hanging.

</details>

<details>
<summary><b>Smooth animation (write-combining)</b></summary>

<br>

Before an OS starts, firmware leaves the screen memory uncached, so a full
frame on a high resolution panel can take over 100 ms to show. Aarambh maps
the framebuffer write-combining through the CPU's page attribute table, the
way Linux does once it runs, and restores the firmware's memory types before
it starts any system.

The first boot after installing records its progress in
`\EFI\aarambh\wc.state`:

| `wc.state`    | Meaning |
|---------------|---------|
| `trying`      | First attempt is running |
| `ok`          | Frames were shown with write-combining; used on every boot |
| `failed`      | A boot found `trying` left over (the attempt hung), so it stays off |
| `unsupported` | The CPU or firmware cannot use it (no PAT, CET, 5-level paging) |

If the menu ever hangs on that first boot, hold the power button: the next boot
turns write-combining off by itself. Delete `wc.state` to try again.

`scripts/install.sh` adds `write_combining no` inside virtual machines other
than QEMU/KVM and on Apple or coreboot firmware, where it is untested. Add
`write_combining no` or `yes` to `aarambh.conf` yourself to override it.

</details>

<details>
<summary><b>Uninstall</b></summary>

<br>

On each Linux installation that ran `setup.sh`:

```sh
sudo bash /boot/efi/EFI/aarambh/hooks/uninstall.sh
```

This removes its menu entry, its files on the ESP, every file listed in
`/etc/aarambh/manifest`, restores every `*.aarambh-backup` (GRUB defaults,
Plymouth and initramfs settings), switches the Plymouth theme back, and
regenerates the initramfs and `grub.cfg`.

On the last installation, add `--purge` to also delete `\EFI\aarambh\` and the
"Aarambh" firmware boot entry:

```sh
sudo bash /boot/efi/EFI/aarambh/hooks/uninstall.sh --purge
```

Both accept `--dry-run`. `make uninstall` runs the first command from the repo.

</details>

---

## Using Aarambh

<details>
<summary><b>Keyboard</b></summary>

<br>

| Key                 | Action                         |
|---------------------|--------------------------------|
| Left / Right        | Switch entry                   |
| 1 - 9, Home, End    | Jump to entry                  |
| Enter / Space       | Start                          |
| Down, then Enter    | Open advanced options          |
| Up / Down           | Choose a kernel in the list    |
| Esc / Backspace     | Close advanced options         |
| E                   | Edit boot options (see below)  |

A kernel picked under advanced options is used for that one boot. If Aarambh
cannot hand the choice to the OS's boot loader, it shows **Could not save the
kernel choice** and starts nothing, instead of booting a different kernel.

Themes that list OSes vertically swap the arrows: Up / Down switch entry and
Right opens the kernel list. Every theme shows the keys that work right now
at the bottom of the screen. Any key press stops the countdown.

**Editing boot options (E).** Like GRUB's `e`, E opens the kernel command line
of the selected entry, or of the kernel highlighted in the list, for one boot.

| Key in the editor   | Action                                    |
|---------------------|-------------------------------------------|
| Enter               | Boot with the edited command line         |
| Esc                 | Cancel                                    |
| Ctrl+R              | Restore the original command line         |
| Left / Right, Home / End | Move the cursor                      |
| Backspace / Delete  | Delete before / at the cursor             |

Nothing is saved: the next boot uses the normal command line again. To change
it for good, see **Kernel command line for direct boot**. E works only for
kernels Aarambh starts itself; Windows, Firmware and entries booted through
GRUB do not show it (use GRUB's own `e` there). An edited boot never falls
back to GRUB, because that would silently drop the edit.

Anyone at the keyboard can use E to boot with `init=/bin/bash` and get a root
shell, as with an unprotected GRUB. Turn it off with `edit no` in
`aarambh.conf`.

</details>

<details>
<summary><b>Configuration (<code>aarambh.conf</code>)</b></summary>

<br>

The file lives at `\EFI\aarambh\aarambh.conf` and is read at every boot.
`setup.sh` adds one `entry` line per Linux installation; you can edit titles,
colors and order by hand.

```
theme accordion
timeout 10
default ubuntu
autodetect yes

entry Ubuntu    | \EFI\ubuntu\shimx64.efi | logos/ubuntu.png   | backgrounds/ubuntu.jpg   | ubuntu | #e95420
entry Firmware  | @firmware               | logos/settings.png | backgrounds/settings.jpg |        | #8a90a8
```

| Key            | Meaning                                               |
|----------------|-------------------------------------------------------|
| `theme`        | Theme name, see **Themes** below                      |
| `timeout`      | Seconds before the default entry starts, `0` to `999`; a larger number counts as `999` and anything that is not a number keeps the default of `10`. `0` disables the countdown (the menu then waits for a key; it does not boot at once). Only an operating system is started by the countdown, never Restart, Power Off or Firmware |
| `default`      | Default entry: OS id (`ubuntu`, `fedora`, `windows`), menu title, or index starting at 0 |
| `autodetect`   | `yes` adds loaders found under `\EFI\` (Windows, other OSes) |
| `edit`         | `yes` (default) lets E edit boot options, `no` turns it off |
| `write_combining` | `yes` (default) maps the screen write-combining for smooth animation, `no` keeps the firmware's slower path (see **Smooth animation** under *Safety and recovery*) |
| `font_light`, `font_regular`, `font_bold`, `font_mono` | Paths of TrueType fonts (`.ttf`). A font that is missing, damaged or built wrong (cut short, wrong checksum, tables that point outside the file, composite glyphs that loop) is not used; its text is drawn with one of the other fonts. Fonts with CFF outlines (most `.otf` files) are not supported |
| `utc_offset`   | Written by the hooks, e.g. `+0530`                    |
| `entry`        | `title \| loader \| logo \| background \| key \| accent` |

Loaders can also be `@firmware`, `@reboot` or `@shutdown`. An entry whose
loader file does not exist is skipped. Operating systems are always listed
before Firmware, Restart and Power Off. When no operating system is left, the
menu shows **No bootable system found** and waits for a key.

</details>

<details>
<summary><b>Plymouth and themes</b></summary>

<br>

`setup.sh` installs one Plymouth theme, `aarambh`, on each OS. At every direct
boot Aarambh replaces its background, dots and script for that boot only, with
the launch splash of the theme set in `aarambh.conf`. Nothing on the disk
changes and the archive lives only in RAM (about 3 MB at 1280x800, 6 MB at
1920x1080).

Restart and shutdown follow the theme too. Plymouth runs from the OS's own
disk then, so when a Linux entry starts after a theme or resolution change,
Aarambh writes that OS's "Restarting" and "Shutting down" frames to
`\EFI\aarambh\shutdown\` (about 4 MB at 1280x800, 8 MB at 1920x1080 per
OS). `aarambh-clear`, which runs at every boot, copies them into the installed
`aarambh` Plymouth theme. No initramfs rebuild is needed. A shutdown right
after `aarambh --theme`, before booting once with the new theme, still shows
the old look.
Without these files Plymouth falls back to the splash `setup.sh` installed.
On systems that return to the initramfs at shutdown (dracut, used by Fedora
and Ubuntu 25.10 and later, or mkinitcpio's shutdown ramfs), Plymouth reloads
its theme from that initramfs copy late in the shutdown.
`aarambh-shutdown-splash.service` copies the current theme into it first, so
the themed splash stays until power off.

Limits:

- Only direct boots get the themed boot splash. A boot that falls back to
  GRUB shows the theme that `setup.sh` installed (restart and shutdown still
  follow the theme).
- After a theme change, restart and shutdown keep the old look until the OS
  has booted once with the new theme: the frames are drawn by Aarambh at
  boot and copied by `aarambh-clear` while the OS starts. So
  `aarambh --theme cinema` followed straight by a shutdown still shows the
  previous theme; every shutdown after the next boot shows Cinema.
- The frame is drawn at the firmware's screen resolution. If Plymouth runs at
  a different aspect ratio, the frame is scaled to cover the screen.

</details>

<details>
<summary><b>Command line (<code>aarambh</code>)</b></summary>

<br>

`setup.sh` installs `/usr/local/bin/aarambh` on every connected OS. Run it as
a normal user: the EFI partition is readable only by root, so it asks for your
password through `sudo` by itself. It edits
`\EFI\aarambh\aarambh.conf` on the shared EFI partition, so a change made from
Ubuntu also applies when you boot from Fedora, and the other way round.

```sh
aarambh --themes                      # list themes, * marks the current one
aarambh --theme cinema                # switch theme
aarambh --list                        # list menu entries, * marks the default
aarambh --default fedora              # boot Fedora by default
aarambh --default windows             # works for autodetected loaders too
aarambh --timeout 5                   # 5 second countdown, 0 disables it
aarambh -t cinema -d ubuntu -T 3      # several at once
aarambh                               # show theme, default and timeout
```

`--default` takes the OS id (the `ID=` from `/etc/os-release`), the menu
title, or an index counted from 0. An id or title keeps pointing at the same
OS when a new installation adds an entry and the order changes; an index does
not.

Only an operating system that is really in the menu can be the default: one
whose boot loader exists on the EFI partition (a folder under `\EFI\` with no
loader Aarambh recognizes is not), and with `autodetect no` only the entries
in `aarambh.conf`. Restart, Power Off and Firmware cannot be the default. The
command refuses anything else and lists the entries, so it never reports a
change that the menu would ignore.

</details>

<details>
<summary><b>Custom logos</b></summary>

<br>

1. Find the id of the distribution (see **Distribution ids for logos**).
2. Save the logo as `assets/custom/<id>-logo.svg` (or `.png`). A single-color
   logo on a transparent background looks best; it is turned into a white
   logo like the built-in ones.
3. Run `make assets`. This creates `logos/<id>.png`, `backgrounds/<id>.jpg`
   (in the distribution's color from `assets/distros.conf`) and
   `plymouth/<id>/`.
4. Run `make install` again to copy them to the ESP, then
   `sudo bash /boot/efi/EFI/aarambh/hooks/setup.sh` on that distribution to
   install its Plymouth theme.

Each `plymouth/<id>/` folder holds only images. The Plymouth script and theme
file exist once, in `assets/plymouth/template/`; `make install` copies them
into every OS folder on the ESP, so a script change reaches every OS without
touching the folders.

`assets/custom/ubuntu-logo.svg` replaces a built-in logo the same way. A
distribution missing from `distros.conf` can be added there with one line.

Needs `imagemagick`, DejaVu fonts and `librsvg2-bin` (`rsvg-convert`).

</details>

<details>
<summary><b>Boot splash on Arch-based systems</b></summary>

<br>

mkinitcpio only includes Plymouth when its hook is enabled, and Aarambh does
not edit `mkinitcpio.conf` for you. To get the splash:

1. Add `plymouth` to `HOOKS` in `/etc/mkinitcpio.conf`, after `udev` (or
   `systemd`).
2. Add `splash` to your kernel command line (in `/etc/default/grub` or your
   systemd-boot entry).
3. Run `sudo mkinitcpio -P` and `sudo /usr/local/sbin/aarambh-kernels`.

`setup.sh` prints a reminder when the hook is missing.

</details>

<details>
<summary><b>Kernel command line for direct boot</b></summary>

<br>

- BLS systems (Fedora family, systemd-boot): the `options` of each entry. With
  GRUB, variables in them (`$kernelopts`, `$tuned_params`) are filled in from
  `grubenv` the way GRUB does it, and everything else on the line is kept.
- Other systems: the command line of the running kernel, without
  `BOOT_IMAGE=`, `initrd=` and recovery-only options.

The longest command line is 2047 characters, the limit of the kernel itself.
A longer one is not shortened: that kernel starts through GRUB instead.

To use a fixed command line instead, write it to `/etc/aarambh/cmdline` and run
`sudo aarambh-kernels`. Recovery mode always starts through GRUB.

</details>

<details>
<summary><b>Themes</b></summary>

<br>

List them with `make themes` in the source folder, or `aarambh --themes`
once installed (it marks the current one with `*`). Choose a theme when
installing (`make install THEME=cinema`), later with
`aarambh --theme cinema`, or by editing the `theme` line in
`aarambh.conf`. Every theme works with any number of systems, shows the kernel
list and has its own launch splash and matching Plymouth splash.

| Screenshot | `theme` | Look | Switch OS |
|---|---|---|---|
| <img src="docs/screenshots/accordion.jpg" width="280" alt="Accordion"> | `accordion` | **Accordion**: Default. Panels expand and collapse, each OS in its own wallpaper | Left / Right |
| <img src="docs/screenshots/minimal.jpg" width="280" alt="Minimal"> | `minimal` | **Minimal**: Small example theme for theme authors | Left / Right |
| <img src="docs/screenshots/glass-sidebar.jpg" width="280" alt="Glass Sidebar"> | `glass-sidebar` | **Glass Sidebar**: Frosted list on the left, ice cyan on deep blue | Up / Down |
| <img src="docs/screenshots/editorial.jpg" width="280" alt="Editorial"> | `editorial` | **Editorial**: Light theme: magazine layout, ink and vermilion on cream paper | Up / Down |
| <img src="docs/screenshots/editorial-dark.jpg" width="280" alt="Editorial Dark"> | `editorial-dark` | **Editorial Dark**: The same magazine layout, cream ink and vermilion on warm black, easy on the eyes at night | Up / Down |
| <img src="docs/screenshots/cinema.jpg" width="280" alt="Cinema"> | `cinema` | **Cinema**: Letterbox and filmstrip in golden-hour amber | Left / Right |
| <img src="docs/screenshots/honeycomb.jpg" width="280" alt="Honeycomb"> | `honeycomb` | **Honeycomb**: Honey hexagon tiles | Left / Right |
| <img src="docs/screenshots/constellation.jpg" width="280" alt="Constellation"> | `constellation` | **Constellation**: Each OS is a star in an indigo sky | Left / Right |
| <img src="docs/screenshots/picker-wheel.jpg" width="280" alt="Picker Wheel"> | `picker-wheel` | **Picker Wheel**: Spinning wheel picker in graphite and mint | Up / Down |
| <img src="docs/screenshots/dial.jpg" width="280" alt="Dial"> | `dial` | **Dial**: Watch dial and needle, copper on racing green | Left / Right |
| <img src="docs/screenshots/hud.jpg" width="280" alt="HUD"> | `hud` | **HUD**: Sci-fi target lock, lime on black | Up / Down |
| <img src="docs/screenshots/departures.jpg" width="280" alt="Departures"> | `departures` | **Departures**: Airport split-flap board, white flaps and sky blue on charcoal; letters flip when the selection changes | Up / Down |
| <img src="docs/screenshots/gallery.jpg" width="280" alt="Gallery"> | `gallery` | **Gallery**: Cover flow of wallpaper cards that turn and reflect on a dark floor | Left / Right |

In themes switched with Up / Down, Right opens the kernel list. In the others,
Down opens it (Accordion, Minimal and Gallery: Down, then Enter).

Plymouth always continues the current theme's launch splash, with no
initramfs rebuild when you switch (see **Plymouth and themes**). A boot that
falls back to GRUB shows the Accordion-style splash installed by `setup.sh`.

A theme only draws; the core handles config, input, countdown, animation
state and booting. See [docs/THEMES.md](docs/THEMES.md) to write your own. The
smallest working example is `src/themes/minimal/minimal.c`.

</details>

---

## Development

<details>
<summary><b>Requirements</b></summary>

<br>

| Purpose           | Debian / Ubuntu                              | Fedora                                  | Arch                                   |
|-------------------|----------------------------------------------|-----------------------------------------|----------------------------------------|
| Build             | `build-essential gnu-efi curl`               | `gcc make gnu-efi-devel curl`           | `base-devel gnu-efi curl`              |
| Install           | `efibootmgr`                                 | `efibootmgr`                            | `efibootmgr`                           |
| Secure Boot (installed by `setup.sh` when needed) | `shim-signed sbsigntool mokutil openssl` | `shim-x64 sbsigntools mokutil openssl` | `sbsigntools openssl` (with `sbctl` keys) |
| `make test`       | the Build row plus `bash coreutils util-linux openssl sbsigntool` | the Build row plus `bash coreutils util-linux openssl sbsigntools libasan libubsan` | the Build row plus `bash coreutils util-linux openssl sbsigntools` |
| Test in QEMU      | `qemu-system-x86 ovmf mtools dosfstools`     | `qemu-system-x86 edk2-ovmf mtools dosfstools` | `qemu-desktop edk2-ovmf mtools dosfstools` |
| Test in QEMU with Secure Boot (`make qemu SECURE_BOOT=1`) | the QEMU row plus `shim-signed sbsigntool openssl python3-virt-firmware` | the QEMU row plus `shim-x64 sbsigntools openssl python3-virt-firmware` | the QEMU row plus `sbsigntools openssl`, and `pipx install virt-firmware`; pass `SHIM=` and `MOK_MANAGER=` from another distribution |
| Regenerate assets | `imagemagick fonts-dejavu-core librsvg2-bin` | `ImageMagick dejavu-sans-fonts librsvg2-tools` | `imagemagick ttf-dejavu librsvg`  |

Everything at once on Debian or Ubuntu:

```sh
sudo apt install build-essential gnu-efi curl efibootmgr \
    shim-signed sbsigntool mokutil openssl \
    qemu-system-x86 ovmf mtools dosfstools python3-virt-firmware
```

`make test` skips the Secure Boot scenario when `sbsign` or `openssl` is
missing, runs its unit tests without AddressSanitizer when `libasan` is
missing (it says so), and `make qemu SECURE_BOOT=1` names the package it is
missing.

Tested with gnu-efi 3.0.15 (Ubuntu 24.04 package) and gnu-efi 4.0.x.

</details>

<details>
<summary><b>Build and test</b></summary>

<br>

```sh
make            # build/aarambh.efi
make test       # hooks against simulated distributions, failure paths, unit tests
make qemu       # boot the menu in QEMU with sample entries
make assets     # regenerate logos, backgrounds and Plymouth themes
make clean
```

- `make test` runs three things, none of which touches your system:
  `tests/hooks/run.sh` (below), `tests/hooks/failures.sh`, which breaks one
  step at a time (a missing initramfs, a failed copy, a full EFI partition, a
  kernel list that cannot be generated) and checks that the last good state
  survives, and `tests/unit/run.sh`, which builds parts of `aarambh.efi` as
  normal Linux programs and feeds them damaged fonts, failed allocations and
  failed writes.
- `tests/hooks/run.sh` creates fake Ubuntu (also with Secure Boot on), Fedora, Arch
  (GRUB and systemd-boot), openSUSE, Void and an unknown Ubuntu derivative in
  a temporary folder, with
  stub commands, and compares the results with `tests/hooks/expected/`.
  Nothing on your system is touched. `UPDATE=1 make test` rewrites the
  expected files after an intended change.
- `make qemu` builds a throwaway ESP image in `build/qemu/` with Ubuntu and
  Fedora entries plus fake Debian and Windows loaders for autodetect. It never
  touches your real ESP. `tests/qemu/run.sh --no-kvm` works without KVM.
- `make qemu SECURE_BOOT=1` (`tests/qemu/run.sh --secure-boot`) boots it like
  a real PC with Secure Boot on: OVMF with Microsoft's keys, your installed
  shim, and Aarambh signed with a throwaway key that is already in MOK. It
  needs `sbsigntool`, `openssl` and `virt-fw-vars` (`python3-virt-firmware`,
  or `pip install virt-firmware`). Pass `SHIM=<file>` and
  `MOK_MANAGER=<file>` when they are not found on their own.

</details>

<details>
<summary><b>Project layout</b></summary>

<br>

```
aarambh/
├── Makefile
├── config/aarambh.conf         Default menu configuration
├── include/aarambh/            Public API for themes (theme.h, widgets.h, theme_kit.h, gfx/)
├── src/
│   ├── main.c                  UEFI entry point
│   ├── core/                   App state, config, entries, autodetect, input, theme registry
│   ├── boot/                   Launcher, direct Linux boot, initrd, one-shot, power actions
│   ├── efi/                    File system, video, timer, loaded image
│   ├── gfx/                    Software renderer
│   ├── ui/                     Widget implementations
│   ├── lib/                    Freestanding memory, string and math helpers
│   └── themes/                 accordion (default), minimal (example) and 9 more
├── third_party/stb/            stb_image and stb_truetype
├── assets/
│   ├── distros.conf            Known distributions: id, name, colors
│   ├── custom/                 Your logos (<id>-logo.svg)
│   └── fonts/ logos/ backgrounds/ plymouth/
├── hooks/
│   ├── setup.sh                Connect a Linux installation
│   ├── uninstall.sh            Undo setup.sh
│   ├── bin/                    aarambh-kernels, aarambh-clear, aarambh-sign
│   ├── lib/                    Detection, kernel lists, GRUB, Plymouth, initramfs
│   ├── triggers/               aarambh-refresh (kernel hook)
│   ├── grub/                   01_aarambh
│   ├── systemd/                aarambh-clear.service, aarambh-shutdown-splash.service
│   └── apt/, pacman/           Copy shim updates next to Aarambh
├── scripts/                    install.sh, make-assets.sh
├── tests/
│   ├── hooks/                  Simulated distributions for setup/uninstall, failure paths
│   ├── unit/                   Parts of aarambh.efi against mocked UEFI services
│   └── qemu/                   Boot the menu in a virtual machine
├── docs/THEMES.md              Writing a theme
├── docs/SECURE_BOOT.md         Installing with Secure Boot on, screen by screen
└── docs/screenshots/           One screenshot per theme, secure-boot/ for the guide
```

</details>

---

## Troubleshooting

Known build and install problems, with the fix for each.

<details>
<summary><b>Reporting a problem: the debug report</b></summary>

<br>

Boot the system normally (if Aarambh does not get you there, use the
firmware boot menu, see *If something goes wrong*), then run:

```sh
aarambh-debug          # installed by setup.sh
make debug             # from the source folder, also works if the install did not finish
```

It asks for your password, changes nothing, and saves
`~/aarambh-debug-<date>.txt`. The first lines are a summary marked
`[PROBLEM]`, `[NOTE]` and `[OK]`; the rest is the raw data behind them:

| Checked                 | What it looks at |
|-------------------------|------------------|
| Platform                | CPU architecture, UEFI or BIOS, 32/64-bit firmware, Secure Boot |
| Secure Boot             | Signing mode, signatures of `aarambh.efi` and the direct-boot kernel, the file shim starts, shim and MokManager next to Aarambh, whether the boot entry starts shim, whether the key is enrolled |
| Installation            | `/etc/aarambh.env`, the `aarambh` command and whether it is in your `PATH`, sudo's `secure_path` |
| EFI partition           | `\EFI\aarambh\` files, `aarambh.conf`, the kernel lists, free space, the menu entry for this OS |
| Direct boot             | Whether the copied kernel still has its modules in `/lib/modules`, `root=` points at a real filesystem, the initrd is not empty |
| Firmware entries        | `efibootmgr -v`, whether this boot went through Aarambh, whether the entry points at the mounted ESP |
| GRUB, initramfs         | `/etc/default/grub` and its changes, `MODULES=`, mkinitcpio `HOOKS`, whether the Aarambh Plymouth theme is inside the initramfs |
| Plymouth, graphics      | Selected theme, `splash` on the command line, GPU and driver, NVIDIA modesetting, encrypted disks |
| Logs                    | Errors and graphics/boot lines of this boot **and the previous one**, failed units, `setup.sh --dry-run` |

Your user name and host name are replaced with `<user>` and `<host>`. Read
the file before you attach it to an
[issue](https://github.com/NarkAgni/aarambh/issues/new).

If the summary says logs of earlier boots are not kept, turn them on, reproduce
the problem once, and run it again:

```sh
sudo mkdir -p /var/log/journal && sudo systemctl restart systemd-journald
```

</details>

<details>
<summary><b><code>aarambh: command not found</code> after installing</b></summary>

<br>

**Causes:**

1. Version 0.1.0 installed the command to `/usr/local/sbin`, which is not in
   a normal user's `PATH` on Debian, openSUSE and some other distributions.
2. `./scripts/install.sh --theme ...` in 0.1.0 passed `--theme` on to
   `setup.sh`, which stopped before installing anything for this OS.
3. `setup.sh` stopped with an error (for example
   `could not find the EFI loader`), so it never reached the step that
   installs the command.

**Fix:** pull the latest version and run the installer again. It now installs
`aarambh` and `aarambh-debug` to `/usr/local/bin` and removes the old copy:

```sh
cd aarambh && git pull
make install
```

On every other Linux on the same disk, run `setup.sh` again (see step 6 of
*Install*). If it still fails, run `make debug`.

Run `aarambh` **without** `sudo`; it asks for your password itself. On
Fedora/RHEL-like systems `sudo aarambh` can still say *command not found*,
because sudo there only searches `/usr/bin` and `/usr/sbin`. The full path
always works: `sudo /usr/local/bin/aarambh`.

</details>

<details>
<summary><b>Black screen after installing</b></summary>

<br>

**First, get back into Linux:**

1. Wait about a minute. If the disk is encrypted, the black screen may be
   the password prompt: type the password and press **Enter**, or press
   **Esc** to show the text prompt.
2. Otherwise hold the power button to switch off, power on and open the
   firmware boot menu (**F12**, **F8**, **F11** or **Esc**). Choose your
   distribution's own entry (`ubuntu`, `fedora`, ...). That boots the way it
   did before Aarambh.

**Then run `aarambh-debug`** (or `make debug`) and attach the report. It
reads the log of the boot that went black. The usual causes, all of which it
checks:

| Summary line                                      | Fix |
|---------------------------------------------------|-----|
| NVIDIA driver without `nvidia-drm.modeset=1`      | `echo 'options nvidia-drm modeset=1' \| sudo tee /etc/modprobe.d/nvidia-drm-modeset.conf`, then rebuild the initramfs (below) |
| Direct-boot kernel has no modules in `/lib/modules` | `sudo /usr/local/sbin/aarambh-kernels` copies the current kernel again |
| `root=` points to a filesystem that does not exist | Write the full, correct command line to `/etc/aarambh/cmdline`, then `sudo /usr/local/sbin/aarambh-kernels` |
| No Aarambh Plymouth theme inside the initramfs    | Rebuild it: `sudo update-initramfs -u` (Debian, Ubuntu), `sudo dracut -f` (Fedora, openSUSE) or `sudo mkinitcpio -P` (Arch). The direct-boot copy is refreshed automatically |
| `MODULES=dep` and the root disk is not found      | Set `MODULES=most` in `/etc/initramfs-tools/initramfs.conf`, then `sudo update-initramfs -u` |
| Secure Boot is on but Aarambh is not signed       | Install your distribution's shim package (`shim-signed`, `shim-x64`, `shim`), then run `setup.sh` again |
| The signing key is not enrolled                   | `sudo aarambh-sign`, then restart and enroll it in MokManager |

To go back to the old boot completely, see **Uninstall** under *Safety and
recovery*.

</details>

<details>
<summary><b><code>'gEfiLoadFile2ProtocolGuid' redeclared as different kind of symbol</code></b></summary>

<br>

```
/usr/include/efi/efilib.h:108:27: error: 'gEfiLoadFile2ProtocolGuid' redeclared as different kind of symbol
  108 | #define LoadFile2Protocol gEfiLoadFile2ProtocolGuid
src/boot/initrd.c:11:3: note: in expansion of macro 'LoadFile2Protocol'
```

**Cause:** gnu-efi 4.x defines `LoadFile2Protocol` as a macro in `efilib.h`.
Any type or variable with that name gets replaced by the GUID symbol.

**Fix:** fixed in `src/boot/initrd.c`, where the protocol struct is now named
`InitrdLoadFile2`. In new code, do not name anything after gnu-efi protocol
aliases (`LoadFile2Protocol`, `LoadedImageProtocol`, `DevicePathProtocol`,
`GraphicsOutputProtocol`, ...).

</details>

<details>
<summary><b><code>crt0-efi-x86_64.o not found</code> / <code>elf_x86_64_efi.lds not found</code></b></summary>

<br>

**Cause:** gnu-efi is not installed.

**Fix:** install it with the command for your distribution in
[step 2 of Install](#step-2-install-the-tools-needed-to-build-it).

</details>

<details>
<summary><b><code>Permission denied</code> when running a script</b></summary>

<br>

**Cause:** the executable bit was lost when the files were downloaded or
copied from a FAT partition.

**Fix:**

```sh
chmod +x scripts/*.sh tests/*/run.sh hooks/*.sh hooks/bin/* hooks/triggers/* hooks/grub/01_aarambh
```

</details>

<details>
<summary><b><code>this system was booted in legacy BIOS mode</code> / <code>no EFI system partition found</code></b></summary>

<br>

**Cause:** the machine booted in BIOS (CSM) mode, or the ESP is not mounted.

**Fix:** check with `ls /sys/firmware/efi` (must exist) and `findmnt -t vfat`.
Switch the firmware to UEFI mode, or mount the ESP. If it is mounted somewhere
other than `/boot/efi`, `/efi` or `/boot`, run with
`AARAMBH_ESP=/your/mountpoint`.

</details>

<details>
<summary><b><code>could not find the EFI loader of ...</code></b></summary>

<br>

**Cause:** `setup.sh` could not tell which folder under `\EFI\` belongs to this
installation (for example a custom `grub-install --bootloader-id`, or a boot
loader other than GRUB and systemd-boot).

**Fix:** look at the folders and pass the right file:

```sh
ls /boot/efi/EFI/*/
sudo bash /boot/efi/EFI/aarambh/hooks/setup.sh --loader '\EFI\MyLinux\grubx64.efi'
```

</details>

<details>
<summary><b><code>not enough space on the EFI partition, direct boot disabled</code></b></summary>

<br>

**Cause:** the newest kernel and initramfs do not fit on the ESP with 8 MB to
spare, even after the old copy was deleted.

**Effect:** nothing breaks. That OS boots through its own loader instead.

**Fix:** free space on the ESP or make the initramfs smaller (initramfs-tools:
`MODULES=dep` is already set; dracut: `hostonly="yes"`), then run
`sudo aarambh-kernels`.

</details>

<details>
<summary><b>Other <code>direct boot disabled</code> warnings</b></summary>

<br>

`aarambh-kernels` prints one of these when it lists a kernel without direct
boot. Nothing breaks in any of them: that kernel starts through the OS's own
boot loader, as it did before Aarambh.

| Warning                                         | Cause and fix |
|-------------------------------------------------|---------------|
| `... names an initrd that does not exist`       | The boot entry lists an initramfs or microcode file that is gone. Rebuild the initramfs (`sudo dracut -f`, `sudo mkinitcpio -P`) or reinstall the microcode package, then `sudo aarambh-kernels` |
| `... uses $kernelopts, which grubenv does not set` | Fedora or RHEL entry that relies on a GRUB variable with no value. `sudo grub2-mkconfig -o /boot/grub2/grub.cfg` writes it again |
| `... has a path or a command line too long for Aarambh` | More than 2047 characters of kernel options, or more than four initramfs parts. Shorten the options, or keep starting that kernel through GRUB |
| `copying or signing the kernel failed`          | The EFI partition is read-only or has errors, or the signing key is unreadable. Check `findmnt -t vfat` and `sudo aarambh-debug` |
| `the kernel command line could not be read from /proc/cmdline` | `aarambh-kernels` ran where `/proc` is not mounted (a chroot). Run it again from the booted system |

</details>

<details>
<summary><b><code>the boot menu was not refreshed</code> during a kernel update</b></summary>

<br>

**Cause:** `aarambh-kernels` failed while a package was being installed. The
update itself is not affected, and the menu keeps the kernel list of the last
run that worked.

**Fix:** read the reason and run it by hand:

```sh
journalctl -t aarambh | tail -n 20
sudo aarambh-kernels
```

</details>

<details>
<summary><b>The menu says <i>No bootable system found</i></b></summary>

<br>

**Cause:** none of the loaders in `aarambh.conf` exists and autodetect found
no other one under `\EFI\` (wrong EFI partition, or the loaders were
deleted). The menu waits for a key; it never restarts by itself.

**Fix:** start your OS from the firmware boot menu (**F12**, **F8**, **F11**
or **Esc** at power on), then run `aarambh-debug`. Running `setup.sh` again
writes the entry of that OS back.

</details>

<details>
<summary><b>The menu says <i>Could not save the kernel choice, nothing was started</i></b></summary>

<br>

**Cause:** you picked a kernel under *Advanced options* that starts through
GRUB or systemd-boot, and Aarambh could not store the choice for it: the EFI
partition is full, read-only or damaged, or the firmware refused the variable.
Starting the loader anyway would have booted its default kernel instead of
yours, so nothing was started.

**Fix:** press **Enter** on the entry itself to start the default kernel, then
free space on the EFI partition (`df -h /boot/efi`) or check it with
`sudo fsck.vfat -n` on its device.

</details>

<details>
<summary><b>Two installations show the same entry, or one is missing</b></summary>

<br>

**Cause:** both use the same `ID` (for example two Ubuntu installs) or the same
`\EFI\<dir>\` (Linux Mint also installs into `\EFI\ubuntu\`).

**Fix:** pass a different loader with `--loader`, or edit the two `entry` lines
in `aarambh.conf` by hand.

</details>

<details>
<summary><b>Blue screen: <code>Verification failed: (0x1A) Security Violation</code></b></summary>

<br>

**Cause:** Secure Boot is on and shim does not trust the key Aarambh is
signed with yet. Shim then opens MokManager.

**Fix:** pick **Continue boot** (or wait); the firmware starts your OS's own
boot entry. In the OS run:

```sh
sudo aarambh-sign             # asks shim to enroll the key
sudo aarambh-sign --password  # the password MokManager will ask for
```

Restart and choose **Enroll MOK → Continue → Yes**, type the password,
**Reboot**.

</details>

<details>
<summary><b><code>Verification failed: (0x1A) Security Violation</code> right after enrolling the key</b></summary>

<br>

**Cause:** Aarambh 0.1.3 before this fix could pick Ubuntu's DKMS key
(`/var/lib/shim-signed/mok/MOK.der`). That key is marked for kernel modules,
and shim refuses boot loaders signed with it even when it is enrolled.
`aarambh-debug` reports it as a kernel-module key.

**Fix:** update Aarambh and run `make install` again. It makes a key of its
own, signs Aarambh with it and asks shim to enroll it; enroll it at the next
restart as before. The DKMS key stays enrolled, which is fine: your drivers
keep using it.

</details>

<details>
<summary><b>The blue <i>Perform MOK management</i> screen comes back at every restart</b></summary>

<br>

**Cause:** the enrollment was not confirmed (the screen timed out, or
**Continue boot** was picked), and with Secure Boot on Aarambh asks again at
every start, because shim cannot start it without the key.

**Fix:** at the next restart choose **Enroll MOK → Continue → Yes** and type
the password from `sudo aarambh-sign --password`. If you would rather not use
Secure Boot, turn it off in the firmware setup; Aarambh keeps working and
stops asking.

</details>

<details>
<summary><b>MokManager says the password is wrong</b></summary>

<br>

**Cause:** an old password, or the number keys on the numeric keypad with
Num Lock off.

**Fix:** read the current one with `sudo aarambh-sign --password` and type it
with the number row above the letters. After three wrong tries MokManager
restarts the PC; the request stays and the screen comes back.

</details>

<details>
<summary><b>The menu says <i>Blocked by Secure Boot</i></b></summary>

<br>

**Cause:** shim refused the kernel or loader of that entry: it is signed with
a key the PC does not trust, or not signed at all. Aarambh then starts the OS's
own boot loader.

**Fix:** run `setup.sh` once on that OS, so its kernel copy is signed with its
key. For a custom kernel, sign it with your MOK key, or let `aarambh-kernels`
copy and sign it.

</details>

<details>
<summary><b><code>sbsigntools and mokutil could not be installed</code></b></summary>

<br>

**Cause:** `setup.sh` found shim and tried to install the signing tools with
your package manager, which failed (no network, or a package name it does not
know).

**Fix:** install them yourself and run `setup.sh` again:

| Distribution        | Command |
|---------------------|---------|
| Debian, Ubuntu      | `sudo apt install sbsigntool mokutil` |
| Fedora              | `sudo dnf install sbsigntools mokutil` |
| openSUSE            | `sudo zypper install sbsigntools mokutil` |
| Arch                | `sudo pacman -S sbsigntools mokutil` |

</details>

<details>
<summary><b><code>Could not access KVM kernel module</code> in <code>make qemu</code></b></summary>

<br>

**Cause:** KVM is not available (inside a VM, or virtualization is disabled
in the firmware).

**Fix:** run without KVM. It is slower but works:

```sh
./tests/qemu/run.sh --no-kvm
```

</details>

---

## License

Aarambh is free software, licensed under the
[GNU General Public License v3.0 or later](LICENSE). You may use, study, share
and change it; if you distribute a modified version, it must stay under the
same license with its source available.

Bundled third-party code and fonts keep their own licenses, listed in
[THIRD_PARTY.md](THIRD_PARTY.md). Operating system names and logos are
trademarks of their owners and are not covered by this license; see
[assets/custom/LOGOS.md](assets/custom/LOGOS.md). Aarambh is not affiliated
with any of them.

## Credits

- [stb](https://github.com/nothings/stb) by Sean Barrett (public domain / MIT)
- [DejaVu fonts](https://dejavu-fonts.github.io/) (Bitstream Vera license)