#!/bin/bash
#
# Run hooks/setup.sh and hooks/uninstall.sh against fake installations of
# several distributions and compare the results with tests/hooks/expected/.
#
# Nothing on the host is touched: every system path is redirected into a
# temporary root with AARAMBH_ROOT, and distribution tools are replaced by
# stubs that only log their arguments.
#
# Usage: tests/hooks/run.sh [scenario...]
#        UPDATE=1 tests/hooks/run.sh   # rewrite the expected files

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EXPECTED="$REPO/tests/hooks/expected"
WORK="$(mktemp -d)"
FAILURES=0

HOST_TOOLS="bash sh sed awk grep cat cp mv rm mkdir install stat cmp df sort cut tr head tail
            basename dirname realpath mktemp sync date touch tac ls id env find wc chmod ln
            readlink printf true false rmdir od sha256sum uname"
SIGNING_TOOLS="openssl sbsign sbverify"

trap 'rm -rf "$WORK"' EXIT

fail() {
    echo "  FAIL: $*"
    FAILURES=$((FAILURES + 1))
}

pass() {
    echo "  ok: $*"
}

# expect_initrd <key> <expected content>: the merged direct-boot initrd.
expect_initrd() {
    local file="$ESP/EFI/aarambh/linux/$1/initrd.img"

    if [ "$(cat "$file" 2>/dev/null)" = "$2" ]; then
        pass "direct-boot initrd has the expected parts"
    else
        fail "direct-boot initrd content: $(cat "$file" 2>/dev/null | tr '\n' ' ')"
    fi
}

expect_no_copy() {
    if [ -d "$ESP/EFI/aarambh/linux/$1" ]; then
        fail "kernel copied although it is already on the ESP"
    else
        pass "kernels used in place on the ESP"
    fi
}

# make_sandbox <dir> <stub...>: PATH with host basics plus logging stubs.
make_sandbox() {
    local dir=$1
    local tool

    shift
    mkdir -p "$dir"

    for tool in $HOST_TOOLS; do
        ln -sf "$(command -v "$tool")" "$dir/$tool"
    done

    for tool in "$@"; do
        cat > "$dir/$tool" <<STUB
#!/bin/sh
echo "$tool \$*" >> "\$STUB_LOG"
case "$tool" in
    grub-probe|grub2-probe) echo 1111-root ;;
    update-alternatives) exit 1 ;;
esac
exit 0
STUB
        chmod +x "$dir/$tool"
    done
}

new_root() {
    ROOT_DIR="$WORK/$1/root"
    BIN_DIR="$WORK/$1/bin"
    STUB_LOG="$WORK/$1/commands.log"

    mkdir -p "$ROOT_DIR"/etc/default "$ROOT_DIR"/boot "$ROOT_DIR"/proc "$ROOT_DIR"/run/systemd/system
    : > "$STUB_LOG"
}

os_release() {
    printf 'NAME="%s"\nID=%s\nID_LIKE="%s"\n' "$1" "$2" "$3" > "$ROOT_DIR/etc/os-release"
}

new_esp() {
    ESP="$1"
    mkdir -p "$ESP/EFI/aarambh"
    cp "$REPO/config/aarambh.conf" "$REPO/assets/distros.conf" "$ESP/EFI/aarambh/"
    mkdir -p "$ESP/EFI/aarambh/plymouth"
    cp -r "$REPO/assets/plymouth/ubuntu" "$ESP/EFI/aarambh/plymouth/"
    cp "$REPO/assets/plymouth/template/aarambh.plymouth" \
        "$REPO/assets/plymouth/template/aarambh.script" "$ESP/EFI/aarambh/plymouth/ubuntu/"
}

kernel_files() {
    local file

    for file in "$@"; do
        mkdir -p "$(dirname "$ROOT_DIR/boot/$file")"
        echo "$file" > "$ROOT_DIR/boot/$file"
    done
}

run_setup() {
    PATH="$BIN_DIR" STUB_LOG="$STUB_LOG" AARAMBH_ROOT="$ROOT_DIR" AARAMBH_ESP="$ESP" \
        TMPDIR="$WORK" bash "$REPO/hooks/setup.sh" "$@" > "$WORK/setup.log" 2>&1
}

run_uninstall() {
    PATH="$BIN_DIR" STUB_LOG="$STUB_LOG" AARAMBH_ROOT="$ROOT_DIR" AARAMBH_ESP="$ESP" \
        bash "$REPO/hooks/uninstall.sh" > "$WORK/uninstall.log" 2>&1
}

normalize() {
    sed -e "s|$WORK|<work>|g" "$1"
}

compare() {
    local name=$1
    local actual=$2
    local expected="$EXPECTED/$name"

    if [ "${UPDATE:-0}" = 1 ]; then
        normalize "$actual" > "$expected"
        pass "$name updated"
        return
    fi

    if diff -u "$expected" <(normalize "$actual") > "$WORK/diff"; then
        pass "$name matches"
    else
        fail "$name differs"
        sed 's/^/    /' "$WORK/diff"
    fi
}

check_setup() {
    local key=$1

    if run_setup "${@:2}"; then
        pass "setup.sh finished"
    else
        fail "setup.sh failed"
        sed 's/^/    /' "$WORK/setup.log"
        return 1
    fi

    compare "$SCENARIO.kernels" "$ESP/EFI/aarambh/$key.kernels"
    grep '^entry' "$ESP/EFI/aarambh/aarambh.conf" > "$WORK/entries"
    compare "$SCENARIO.entries" "$WORK/entries"
    compare "$SCENARIO.commands" "$STUB_LOG"
}

expect_plymouth_theme() {
    local installed="$ROOT_DIR/usr/share/plymouth/themes/aarambh/aarambh.script"

    if cmp -s "$REPO/assets/plymouth/template/aarambh.script" "$installed"; then
        pass "Plymouth theme installed with the current script"
    else
        fail "Plymouth theme missing or with an old script"
    fi
}

check_uninstall() {
    local key=$1

    rm -f "$WORK/grub.original"
    cp "$ROOT_DIR/etc/default/grub.aarambh-backup" "$WORK/grub.original" 2>/dev/null || true

    if run_uninstall; then
        pass "uninstall.sh finished"
    else
        fail "uninstall.sh failed"
        sed 's/^/    /' "$WORK/uninstall.log"
        return 1
    fi

    if [ -f "$ROOT_DIR/etc/aarambh.env" ] || [ -d "$ROOT_DIR/usr/local/lib/aarambh" ]; then
        ls -la "$ROOT_DIR/etc/aarambh.env" "$ROOT_DIR/usr/local/lib/aarambh" 2>&1 | sed "s/^/    /"
        fail "files left behind after uninstall"
    else
        pass "installed files removed"
    fi

    if grep -q "| $key " "$ESP/EFI/aarambh/aarambh.conf"; then
        fail "menu entry still present"
    else
        pass "menu entry removed"
    fi

    if [ -f "$WORK/grub.original" ]; then
        if cmp -s "$WORK/grub.original" "$ROOT_DIR/etc/default/grub"; then
            pass "/etc/default/grub restored"
        else
            fail "/etc/default/grub not restored"
        fi
    fi
}

# mokutil stub: no key is enrolled yet; every call is logged.
mokutil_stub() {
    cat > "$BIN_DIR/mokutil" <<'STUB'
#!/bin/sh
echo "mokutil $*" >> "$STUB_LOG"
case "$1" in
    --test-key) echo "$2 is not enrolled" ;;
esac
exit 0
STUB
    chmod +x "$BIN_DIR/mokutil"
}

expect_signed() {
    local cert="$ROOT_DIR/var/lib/aarambh/keys/signing.crt"

    if PATH="$BIN_DIR" sbverify --cert "$cert" "$1" >/dev/null 2>&1; then
        pass "$2 is signed with the generated key"
    else
        fail "$2 is not signed with the generated key"
    fi
}

expect_same() {
    if cmp -s "$1" "$2"; then
        pass "$3"
    else
        fail "$3"
    fi
}

# Ubuntu with Secure Boot on and shim on the ESP, but no MOK key yet: a key
# is generated, Aarambh and the kernel copy are signed, shim is copied next to
# Aarambh and the key is queued for enrollment.
scenario_ubuntu_secure_boot() {
    local efi="$REPO/build/aarambh.efi"
    local tool

    for tool in $SIGNING_TOOLS; do
        if ! command -v "$tool" >/dev/null; then
            echo "  skipped: $tool is not installed (sbsigntool, openssl)"
            return 0
        fi
    done

    if [ ! -f "$efi" ]; then
        echo "  skipped: build/aarambh.efi is missing (run make first)"
        return 0
    fi

    new_root ubuntu-secure-boot
    make_sandbox "$BIN_DIR" update-grub grub-probe update-initramfs systemctl
    for tool in $SIGNING_TOOLS; do
        ln -sf "$(command -v "$tool")" "$BIN_DIR/$tool"
    done
    mokutil_stub

    os_release "Ubuntu" ubuntu debian
    new_esp "$ROOT_DIR/boot/efi"
    cp "$efi" "$ESP/EFI/aarambh/aarambh.efi"
    mkdir -p "$ESP/EFI/ubuntu" "$ROOT_DIR/etc/initramfs-tools" "$ROOT_DIR/etc/apt/apt.conf.d"
    echo shim > "$ESP/EFI/ubuntu/shimx64.efi"
    echo mokmanager > "$ESP/EFI/ubuntu/mmx64.efi"
    mkdir -p "$ROOT_DIR/sys/firmware/efi/efivars"
    printf '\006\000\000\000\001' > "$ROOT_DIR/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c"
    printf 'GRUB_TIMEOUT=10\n' > "$ROOT_DIR/etc/default/grub"
    printf 'MODULES=most\n' > "$ROOT_DIR/etc/initramfs-tools/initramfs.conf"
    : > "$ROOT_DIR/etc/initramfs-tools/modules"
    echo "BOOT_IMAGE=/boot/vmlinuz-6.8.0-45-generic root=UUID=1111-root ro quiet splash" \
        > "$ROOT_DIR/proc/cmdline"
    kernel_files initrd.img-6.8.0-45-generic
    cp "$efi" "$ROOT_DIR/boot/vmlinuz-6.8.0-45-generic"

    # Ubuntu's DKMS key is marked for kernel modules, so shim would refuse
    # Aarambh signed with it: a key of Aarambh's own must be made instead.
    mkdir -p "$ROOT_DIR/var/lib/shim-signed/mok"
    openssl req -new -x509 -newkey rsa:2048 -nodes -days 3650 -subj "/CN=DKMS module key/" \
        -addext "extendedKeyUsage=codeSigning,1.3.6.1.4.1.2312.16.1.2" \
        -keyout "$ROOT_DIR/var/lib/shim-signed/mok/MOK.priv" -outform der \
        -out "$ROOT_DIR/var/lib/shim-signed/mok/MOK.der" 2>/dev/null

    check_setup ubuntu --no-plymouth
    if grep -qx "SIGN_DER=$ROOT_DIR/var/lib/aarambh/keys/aarambh.der" "$ROOT_DIR/etc/aarambh.env"; then
        pass "the kernel-module DKMS key is skipped, Aarambh's own key is used"
    else
        fail "signing key: $(grep '^SIGN_DER=' "$ROOT_DIR/etc/aarambh.env")"
    fi

    expect_signed "$ESP/EFI/aarambh/aarambh.efi" "aarambh.efi"
    expect_same "$ESP/EFI/aarambh/aarambh.efi" "$ESP/EFI/aarambh/grubx64.efi" \
        "grubx64.efi is the signed Aarambh"
    expect_same "$ESP/EFI/ubuntu/shimx64.efi" "$ESP/EFI/aarambh/shimx64.efi" "shim copied"
    expect_same "$ESP/EFI/ubuntu/mmx64.efi" "$ESP/EFI/aarambh/mmx64.efi" "MokManager copied"
    expect_signed "$ESP/EFI/aarambh/linux/ubuntu/vmlinuz" "the direct-boot kernel"

    local password="$ROOT_DIR/var/lib/aarambh/keys/mok-password"

    if grep -q 'Enroll MOK' "$WORK/setup.log" && grep -qE '^[0-9]{8}$' "$password" \
        && grep -qF "password: $(cat "$password")" "$WORK/setup.log"; then
        pass "enrollment steps shown with an 8-digit password"
    else
        fail "enrollment steps or the 8-digit password missing"
    fi

    if [ "$(stat -c %a "$password")" = 600 ]; then
        pass "the password file is root-only"
    else
        fail "the password file is readable by others"
    fi

    check_uninstall ubuntu
}

scenario_ubuntu() {
    new_root ubuntu
    make_sandbox "$BIN_DIR" update-grub grub-probe update-initramfs systemctl plymouthd \
        plymouth-set-default-theme update-alternatives kernel-install
    os_release "Ubuntu" ubuntu debian
    new_esp "$ROOT_DIR/boot/efi"
    mkdir -p "$ESP/EFI/ubuntu" "$ROOT_DIR/etc/initramfs-tools" "$ROOT_DIR/etc/kernel/postinst.d"
    mkdir -p "$ROOT_DIR/usr/lib/x86_64-linux-gnu/plymouth" "$ROOT_DIR/sys/module/i915"
    touch "$ESP/EFI/ubuntu/shimx64.efi" "$ROOT_DIR/usr/lib/x86_64-linux-gnu/plymouth/script.so"
    printf 'GRUB_DEFAULT=0\nGRUB_TIMEOUT=10\nGRUB_TIMEOUT_STYLE=menu\nGRUB_CMDLINE_LINUX_DEFAULT="quiet splash"\n' \
        > "$ROOT_DIR/etc/default/grub"
    printf 'MODULES=most\n' > "$ROOT_DIR/etc/initramfs-tools/initramfs.conf"
    : > "$ROOT_DIR/etc/initramfs-tools/modules"
    echo "BOOT_IMAGE=/boot/vmlinuz-6.8.0-45-generic root=UUID=1111-root ro quiet splash vt.handoff=7" \
        > "$ROOT_DIR/proc/cmdline"
    kernel_files vmlinuz-6.8.0-45-generic initrd.img-6.8.0-45-generic \
                 vmlinuz-6.8.0-40-generic initrd.img-6.8.0-40-generic

    # Restart and shutdown frames Aarambh drew for the chosen theme.
    mkdir -p "$ESP/EFI/aarambh/shutdown"
    for part in bg reboot shutdown dot; do
        echo "cinema $part" > "$ESP/EFI/aarambh/shutdown/ubuntu-$part.png"
    done

    check_setup ubuntu
    expect_plymouth_theme

    if cmp -s "$ESP/EFI/aarambh/shutdown/ubuntu-reboot.png" \
        "$ROOT_DIR/usr/share/plymouth/themes/aarambh/still-reboot.png"; then
        pass "the theme's restart frames survive reinstalling the Plymouth theme"
    else
        fail "the theme's restart frames were lost when the Plymouth theme was reinstalled"
    fi
    expect_initrd ubuntu "initrd.img-6.8.0-45-generic"
    check_uninstall ubuntu
}

scenario_fedora() {
    new_root fedora
    make_sandbox "$BIN_DIR" grub2-mkconfig grub2-probe dracut systemctl plymouthd \
        plymouth-set-default-theme kernel-install grub2-editenv
    os_release "Fedora Linux" fedora ""
    new_esp "$ROOT_DIR/boot/efi"
    mkdir -p "$ESP/EFI/fedora" "$ROOT_DIR/boot/grub2" "$ROOT_DIR/boot/loader/entries"
    touch "$ESP/EFI/fedora/shimx64.efi"
    printf 'GRUB_TIMEOUT=5\nGRUB_ENABLE_BLSCFG=true\n' > "$ROOT_DIR/etc/default/grub"
    echo "BOOT_IMAGE=(hd0,gpt2)/vmlinuz-6.11.4-301.fc41.x86_64 root=UUID=2222 ro rhgb quiet" \
        > "$ROOT_DIR/proc/cmdline"

    local entries="$ROOT_DIR/boot/loader/entries"
    printf 'title Fedora\nversion 6.11.4-301.fc41.x86_64\nlinux /vmlinuz-6.11.4-301.fc41.x86_64\ninitrd /initramfs-6.11.4-301.fc41.x86_64.img\noptions root=UUID=2222 ro rhgb quiet\n' \
        > "$entries/m-6.11.4-301.fc41.x86_64.conf"
    printf 'title Fedora\nversion 6.10.1-200.fc41.x86_64\nlinux /vmlinuz-6.10.1-200.fc41.x86_64\ninitrd /initramfs-6.10.1-200.fc41.x86_64.img\noptions root=UUID=2222 ro\n' \
        > "$entries/m-6.10.1-200.fc41.x86_64.conf"
    printf 'title Fedora rescue\nversion 0-rescue-abc\nlinux /vmlinuz-0-rescue-abc\ninitrd /initramfs-0-rescue-abc.img\noptions root=UUID=2222 ro\n' \
        > "$entries/m-0-rescue-abc.conf"
    kernel_files vmlinuz-6.11.4-301.fc41.x86_64 initramfs-6.11.4-301.fc41.x86_64.img

    check_setup fedora --no-plymouth
    check_uninstall fedora
}

scenario_arch_systemd_boot() {
    new_root arch-systemd-boot
    make_sandbox "$BIN_DIR" mkinitcpio systemctl kernel-install bootctl
    os_release "Arch Linux" arch ""
    ESP="$ROOT_DIR/boot"
    mkdir -p "$ESP/EFI/aarambh/plymouth" "$ESP/EFI/systemd" "$ESP/loader/entries"
    cp "$REPO/config/aarambh.conf" "$REPO/assets/distros.conf" "$ESP/EFI/aarambh/"
    touch "$ESP/EFI/systemd/systemd-bootx64.efi"
    echo "0123456789abcdef" > "$ROOT_DIR/etc/machine-id"
    echo "initrd=\\intel-ucode.img initrd=\\initramfs-linux.img root=UUID=3333 rw quiet" \
        > "$ROOT_DIR/proc/cmdline"
    kernel_files vmlinuz-linux initramfs-linux.img intel-ucode.img
    printf 'title Arch Linux\nlinux /vmlinuz-linux\ninitrd /intel-ucode.img\ninitrd /initramfs-linux.img\noptions root=UUID=3333 rw quiet\n' \
        > "$ESP/loader/entries/arch.conf"
    printf 'title Other OS\nlinux /vmlinuz-other\ninitrd /initramfs-other.img\noptions root=UUID=9999 rw\n' \
        > "$ESP/loader/entries/other.conf"

    check_setup arch
    expect_no_copy arch
    check_uninstall arch
}

scenario_arch_grub() {
    new_root arch-grub
    # update-initramfs is present but is the CachyOS wrapper around mkinitcpio,
    # so the expected commands below must stay mkinitcpio's.
    make_sandbox "$BIN_DIR" grub-mkconfig grub-probe mkinitcpio update-initramfs \
        systemctl plymouthd plymouth-set-default-theme
    os_release "Arch Linux" arch ""
    new_esp "$ROOT_DIR/efi"
    mkdir -p "$ESP/EFI/GRUB" "$ROOT_DIR/boot/grub" "$ROOT_DIR/usr/lib/plymouth"
    touch "$ESP/EFI/GRUB/grubx64.efi" "$ROOT_DIR/usr/lib/plymouth/script.so"
    printf 'GRUB_TIMEOUT=5\nGRUB_DISABLE_RECOVERY=true\n' > "$ROOT_DIR/etc/default/grub"
    printf 'HOOKS=(base udev autodetect modconf block filesystems fsck)\n' \
        > "$ROOT_DIR/etc/mkinitcpio.conf"
    echo "BOOT_IMAGE=/boot/vmlinuz-linux root=UUID=1111-root rw loglevel=3 quiet" \
        > "$ROOT_DIR/proc/cmdline"
    kernel_files vmlinuz-linux initramfs-linux.img vmlinuz-linux-lts initramfs-linux-lts.img \
                 amd-ucode.img
    cat > "$ROOT_DIR/boot/grub/grub.cfg" <<'CFG'
menuentry 'Arch Linux' --class arch --class gnu-linux $menuentry_id_option 'gnulinux-simple-1111-root' {
	linux	/boot/vmlinuz-linux root=UUID=1111-root rw loglevel=3 quiet
	initrd	/boot/amd-ucode.img /boot/initramfs-linux.img
}
submenu 'Advanced options for Arch Linux' $menuentry_id_option 'gnulinux-advanced-1111-root' {
	menuentry 'Arch Linux, with Linux linux' --class arch $menuentry_id_option 'gnulinux-linux-advanced-1111-root' {
		linux	/boot/vmlinuz-linux root=UUID=1111-root rw loglevel=3 quiet
	}
	menuentry 'Arch Linux, with Linux linux (fallback initramfs)' --class arch $menuentry_id_option 'gnulinux-linux-fallback-1111-root' {
		linux	/boot/vmlinuz-linux root=UUID=1111-root rw loglevel=3 quiet
	}
	menuentry 'Arch Linux, with Linux linux-lts' --class arch $menuentry_id_option 'gnulinux-linux-lts-advanced-1111-root' {
		linux	/boot/vmlinuz-linux-lts root=UUID=1111-root rw loglevel=3 quiet
	}
}
CFG

    check_setup arch
    expect_initrd arch "$(printf 'amd-ucode.img\ninitramfs-linux.img')"
    check_uninstall arch
}

scenario_opensuse() {
    new_root opensuse
    make_sandbox "$BIN_DIR" grub2-mkconfig grub2-probe dracut systemctl
    os_release "openSUSE Tumbleweed" opensuse-tumbleweed "opensuse suse"
    new_esp "$ROOT_DIR/boot/efi"
    mkdir -p "$ESP/EFI/opensuse" "$ROOT_DIR/boot/grub2"
    touch "$ESP/EFI/opensuse/shim.efi" "$ESP/EFI/opensuse/grub.efi"
    printf 'GRUB_TIMEOUT=8\nGRUB_DISABLE_RECOVERY="false"\n' > "$ROOT_DIR/etc/default/grub"
    echo "BOOT_IMAGE=/boot/vmlinuz-6.11.5-1-default root=UUID=1111-root splash=silent quiet" \
        > "$ROOT_DIR/proc/cmdline"
    kernel_files vmlinuz-6.11.5-1-default initrd-6.11.5-1-default

    check_setup opensuse-tumbleweed
    check_uninstall opensuse-tumbleweed
}

scenario_void() {
    new_root void
    make_sandbox "$BIN_DIR" dracut
    rm -rf "$ROOT_DIR/run/systemd"
    os_release "Void" void ""
    new_esp "$ROOT_DIR/boot/efi"
    mkdir -p "$ROOT_DIR/etc/kernel.d/post-install"
    echo "BOOT_IMAGE=/boot/vmlinuz-6.6.58_1 root=UUID=5555 ro quiet" > "$ROOT_DIR/proc/cmdline"
    kernel_files vmlinuz-6.6.58_1 initramfs-6.6.58_1.img

    check_setup void --loader '\EFI\void\grubx64.efi'
    check_uninstall void
}

scenario_derivative() {
    new_root derivative
    make_sandbox "$BIN_DIR" update-grub grub-probe update-initramfs systemctl
    os_release "TUXEDO OS" tuxedo "ubuntu debian"
    new_esp "$ROOT_DIR/boot/efi"
    mkdir -p "$ESP/EFI/ubuntu" "$ROOT_DIR/etc/initramfs-tools"
    touch "$ESP/EFI/ubuntu/shimx64.efi"
    printf 'GRUB_TIMEOUT=10\n' > "$ROOT_DIR/etc/default/grub"
    printf 'MODULES=most\n' > "$ROOT_DIR/etc/initramfs-tools/initramfs.conf"
    : > "$ROOT_DIR/etc/initramfs-tools/modules"
    echo "BOOT_IMAGE=/boot/vmlinuz-6.11.0-108-generic root=UUID=1111-root ro quiet splash" \
        > "$ROOT_DIR/proc/cmdline"
    kernel_files vmlinuz-6.11.0-108-generic initrd.img-6.11.0-108-generic

    check_setup tuxedo
    check_uninstall tuxedo
}

ALL="ubuntu ubuntu_secure_boot fedora arch_systemd_boot arch_grub opensuse void derivative"

for SCENARIO in ${*:-$ALL}; do
    SCENARIO=${SCENARIO//-/_}
    echo "== $SCENARIO"
    SCENARIO=${SCENARIO//_/-}
    "scenario_${SCENARIO//-/_}"
done

echo
if [ "$FAILURES" = 0 ]; then
    echo "all hook tests passed"
else
    echo "$FAILURES hook test(s) failed"
    exit 1
fi