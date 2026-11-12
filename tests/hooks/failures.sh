#!/bin/bash
#
# Failure paths of the hook libraries: what happens when a file is missing, a
# copy fails halfway or the kernel list cannot be generated. Every case runs
# the real functions of hooks/lib and hooks/bin against a temporary root and
# breaks one step on purpose.
#
# Nothing on the host is touched.
#
# Usage: tests/hooks/failures.sh

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="$(mktemp -d)"
FAILURES=0

trap 'rm -rf "$WORK"' EXIT

fail() {
    echo "  FAIL: $*"
    FAILURES=$((FAILURES + 1))
}

pass() {
    echo "  ok: $*"
}

# expect <description> <actual> <expected>
expect() {
    if [ "$2" = "$3" ]; then
        pass "$1"
    else
        fail "$1"
        echo "    expected: $3"
        echo "    actual:   $2"
    fi
}

# expect_status <description> <actual> <expected>
expect_status() {
    expect "$1 (exit status)" "$2" "$3"
}

# A fresh root with an ESP at /boot/efi and the libraries sourced.
new_root() {
    ROOT="$WORK/$1"
    ESP_ROOT="$ROOT/boot/efi"
    ESP_DIR="$ESP_ROOT/EFI/aarambh"
    KEY=test
    KERNEL_LIST="$ESP_DIR/$KEY.kernels"
    LINUX_DIR="$ESP_DIR/linux/$KEY"
    EFI_LINUX_DIR="\\EFI\\aarambh\\linux\\$KEY"
    BOOTLOADER=${2:-grub}
    SIGN_MODE=none
    TMPDIR="$WORK"

    mkdir -p "$ESP_DIR" "$ROOT/boot/loader/entries" "$ROOT/proc" "$ROOT/etc"

    # shellcheck source=/dev/null
    AARAMBH_ROOT="$ROOT" . "$REPO/hooks/lib/common.sh"
    for library in cmdline direct-boot signing kernels-bls kernels-scan; do
        # shellcheck source=/dev/null
        . "$REPO/hooks/lib/$library.sh"
    done
}

# bls_entry <name> <line...>: one entry in /boot/loader/entries.
bls_entry() {
    local file="$ROOT/boot/loader/entries/$1.conf"

    shift
    printf '%s\n' "$@" > "$file"
    echo "$file"
}

boot_files() {
    local file

    for file in "$@"; do
        echo "$file" > "$ROOT/boot/$file"
    done
}

grub_env() {
    GRUB_ENV=$1
    grub2-editenv() { printf '%s\n' "$GRUB_ENV"; }
}

# The direct-boot fields list_bls_kernels would add for an entry.
fields_of() {
    bls_direct_fields "$1" 6.12 1 2>/dev/null
}

group_missing_initrd() {
    local entry

    echo "== an initrd named by the entry is missing"
    new_root missing-initrd
    boot_files vmlinuz microcode.img initramfs.img

    entry=$(bls_entry all-present "linux /vmlinuz" "initrd /microcode.img" "initrd /initramfs.img" \
        "options root=UUID=abc ro")
    case "$(fields_of "$entry")" in
        *"|root=UUID=abc ro") pass "every initrd present: direct boot" ;;
        *) fail "every initrd present: direct boot ($(fields_of "$entry"))" ;;
    esac

    entry=$(bls_entry main-missing "linux /vmlinuz" "initrd /microcode.img" "initrd /gone.img" \
        "options root=UUID=abc ro")
    expect "microcode present, initramfs missing: no direct boot" "$(fields_of "$entry")" ""

    entry=$(bls_entry first-missing "linux /vmlinuz" "initrd /gone.img" "initrd /initramfs.img" \
        "options root=UUID=abc ro")
    expect "microcode missing, initramfs present: no direct boot" "$(fields_of "$entry")" ""

    entry=$(bls_entry all-missing "linux /vmlinuz" "initrd /gone.img" "options root=UUID=abc ro")
    expect "every initrd missing: no direct boot" "$(fields_of "$entry")" ""

    entry=$(bls_entry same-line "linux /vmlinuz" "initrd /microcode.img /gone.img" \
        "options root=UUID=abc ro")
    expect "two initrds on one line, one missing: no direct boot" "$(fields_of "$entry")" ""

    # No initrd at all is a valid kernel; it is only booted in place on the ESP.
    ESP_ROOT="$ROOT/boot"
    entry=$(bls_entry none-declared "linux /vmlinuz" "options root=UUID=abc ro")
    expect "no initrd named: direct boot without one" "$(fields_of "$entry")" \
        "|\\vmlinuz||root=UUID=abc ro"
}

group_grub_variables() {
    local entry

    echo "== GRUB variables in BLS entries"
    new_root grub-variables
    grub_env "kernelopts=root=UUID=abc ro quiet"

    entry=$(bls_entry suffix "options \$kernelopts systemd.unit=rescue.target rd.break")
    expect "options after \$kernelopts are kept" "$(bls_options "$entry")" \
        "root=UUID=abc ro quiet systemd.unit=rescue.target rd.break"

    entry=$(bls_entry prefix "options debug \${kernelopts} rd.break")
    expect "options around \${kernelopts} are kept" "$(bls_options "$entry")" \
        "debug root=UUID=abc ro quiet rd.break"

    entry=$(bls_entry two-lines "options \$kernelopts" "options rd.break")
    expect "several options lines are joined" "$(bls_options "$entry")" \
        "root=UUID=abc ro quiet rd.break"

    entry=$(bls_entry tuned "options root=UUID=abc ro \$tuned_params")
    expect "an unset variable is empty, as in GRUB" "$(bls_options "$entry")" "root=UUID=abc ro"

    entry=$(bls_entry plain "options root=UUID=abc ro price=5\$")
    expect "a dollar sign that names no variable is kept" "$(bls_options "$entry")" \
        "root=UUID=abc ro price=5\$"

    boot_files vmlinuz initramfs.img
    entry=$(bls_entry tuned-initrd "linux /vmlinuz" "initrd /initramfs.img \$tuned_initrd" \
        "options root=UUID=abc ro")
    expect "an unset variable on the initrd line names no file" "$(bls_initrds "$entry")" \
        "$ROOT/boot/initramfs.img"

    grub_env "tuned_params=skew_tick=1"
    entry=$(bls_entry no-kernelopts "linux /vmlinuz" "initrd /initramfs.img" "options \$kernelopts quiet")
    bls_options "$entry" >/dev/null
    expect_status "\$kernelopts missing from grubenv is an error" "$?" 1
    expect "\$kernelopts missing from grubenv: no direct boot" "$(fields_of "$entry")" ""

    BOOTLOADER=systemd-boot
    entry=$(bls_entry literal "options root=UUID=abc \$kernelopts")
    expect "systemd-boot entries are not expanded" "$(bls_options "$entry")" \
        "root=UUID=abc \$kernelopts"
}

owner() {
    if belongs_to_this_os "$1"; then
        echo ours
    else
        echo foreign
    fi
}

group_entry_owner() {
    local entry

    echo "== entries of another installation on a shared ESP"
    new_root entry-owner systemd-boot
    echo "root=/dev/sda1 ro" > "$ROOT/proc/cmdline"

    entry=$(bls_entry sda10 "options root=/dev/sda10 ro")
    expect "/dev/sda1 does not match root=/dev/sda10" "$(owner "$entry")" foreign

    entry=$(bls_entry sda1 "options ro root=/dev/sda1")
    expect "the same root= argument matches" "$(owner "$entry")" ours

    entry=$(bls_entry comment "# root=/dev/sda1" "options root=/dev/sdb2 ro")
    expect "root= in a comment does not match" "$(owner "$entry")" foreign

    echo "root=LABEL=linux ro" > "$ROOT/proc/cmdline"
    entry=$(bls_entry label-old "options root=LABEL=linux-old ro")
    expect "LABEL=linux does not match LABEL=linux-old" "$(owner "$entry")" foreign

    echo "0123456789abcdef0123456789abcdef" > "$ROOT/etc/machine-id"
    entry=$(bls_entry other-machine "machine-id ffffffffffffffffffffffffffffffff" \
        "options root=LABEL=linux ro")
    expect "another machine id wins over a matching root=" "$(owner "$entry")" foreign

    entry=$(bls_entry ffffffffffffffffffffffffffffffff-6.12 "options root=LABEL=linux ro")
    expect "another machine id in the file name wins too" "$(owner "$entry")" foreign

    entry=$(bls_entry 0123456789abcdef0123456789abcdef-6.12 "options root=UUID=else ro")
    expect "this machine's id in the file name matches" "$(owner "$entry")" ours
}

copy_status() {
    copy_for_direct_boot "$@" 2>/dev/null
    echo "$?"
}

copies() {
    ls "$LINUX_DIR" 2>/dev/null | tr '\n' ' '
}

group_kernel_copy() {
    local first

    echo "== copying the kernel to the ESP"
    new_root kernel-copy
    boot_files vmlinuz microcode.img initramfs.img

    expect_status "a good copy succeeds" \
        "$(copy_status "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img")" 0
    copy_for_direct_boot "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img"
    first=$COPIED_KERNEL
    expect "the pair is named after one id" "$(copies)" "${first/vmlinuz/initrd}.img $first "
    expect "the initrd is microcode followed by the initramfs" \
        "$(cat "$LINUX_DIR/$COPIED_INITRD")" "$(printf 'microcode.img\ninitramfs.img')"

    echo "new initramfs" > "$ROOT/boot/initramfs.img"

    mv() { return 1; }
    expect_status "a failed rename is reported" \
        "$(copy_status "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img")" 1
    unset -f mv
    expect "a failed rename leaves only the old pair" "$(copies)" "${first/vmlinuz/initrd}.img $first "

    cp() { return 1; }
    expect_status "a failed copy is reported" \
        "$(copy_status "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img")" 1
    unset -f cp
    expect "a failed copy leaves only the old pair" "$(copies)" "${first/vmlinuz/initrd}.img $first "

    mktemp() { return 1; }
    expect_status "no scratch file is reported" \
        "$(copy_status "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img")" 1
    unset -f mktemp

    cat() { return 1; }
    expect_status "initramfs parts that cannot be joined are reported" \
        "$(copy_status "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img")" 1
    unset -f cat

    esp_free_bytes() { echo 1000; }
    expect_status "a full EFI partition is reported" \
        "$(copy_status "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img")" 1
    unset -f esp_free_bytes
    # shellcheck source=/dev/null
    . "$REPO/hooks/lib/direct-boot.sh"

    expect_status "a missing initramfs is reported" \
        "$(copy_status "$ROOT/boot/vmlinuz" "$ROOT/boot/gone.img")" 1

    copy_for_direct_boot "$ROOT/boot/vmlinuz" "$ROOT/boot/microcode.img" "$ROOT/boot/initramfs.img"
    if [ "$COPIED_KERNEL" != "$first" ] && [ -s "$LINUX_DIR/$COPIED_KERNEL" ] \
        && [ -s "$LINUX_DIR/$COPIED_INITRD" ]; then
        pass "a changed initramfs gets a new pair"
    else
        fail "a changed initramfs gets a new pair ($(copies))"
    fi

    # Files of Aarambh 0.1.5 and of an interrupted run next to the new pair.
    touch "$LINUX_DIR/vmlinuz" "$LINUX_DIR/initrd.img" "$LINUX_DIR/vmlinuz.stamp" \
        "$LINUX_DIR/vmlinuz-000000000000.new"
    echo "Linux|id$(copied_fields "root=UUID=abc")" > "$KERNEL_LIST"
    prune_direct_boot
    expect "only the pair in the kernel list survives" "$(copies)" "$COPIED_INITRD $COPIED_KERNEL "

    echo "Linux|id" > "$KERNEL_LIST"
    prune_direct_boot
    expect "nothing is kept for a list without direct boot" "$(copies)" ""
}

long_text() {
    head -c "$1" /dev/zero | tr '\0' 'x'
}

group_long_values() {
    echo "== values too long for aarambh.efi"
    new_root long-values

    expect "a command line of 2047 characters is listed" \
        "$(direct_fields '\vmlinuz' '\initrd.img' "$(long_text 2047)" 2>/dev/null | wc -c)" \
        "$((2047 + 22))"
    expect "a command line of 2048 characters is not cut, the kernel loses direct boot" \
        "$(direct_fields '\vmlinuz' '\initrd.img' "$(long_text 2048)" 2>/dev/null)" ""
    expect "a kernel path of 160 characters loses direct boot" \
        "$(direct_fields "\\$(long_text 159)" '\initrd.img' "root=UUID=abc" 2>/dev/null)" ""
    expect "five initrd parts lose direct boot" \
        "$(direct_fields '\vmlinuz' '\a,\b,\c,\d,\e' "root=UUID=abc" 2>/dev/null)" ""
    expect "four initrd parts are listed" \
        "$(direct_fields '\vmlinuz' '\a,\b,\c,\d' "root=UUID=abc" 2>/dev/null)" \
        '|\vmlinuz|\a,\b,\c,\d|root=UUID=abc'
}

# override <library> <shell code>: replaces functions in the copied library.
# The code starts on a line of its own, even after a file without a final
# newline, so it can never turn into a syntax error that fails the run for
# the wrong reason.
override() {
    printf '\n%s\n' "$2" >> "$LIBRARIES/$1"
}

# run_generator: hooks/bin/aarambh-kernels against the current root, with the
# libraries from $LIBRARIES (a copy that a case may break).
run_generator() {
    AARAMBH_ROOT="$ROOT" AARAMBH_LIB_DIR="$LIBRARIES" TMPDIR="$WORK" \
        bash "$REPO/hooks/bin/aarambh-kernels" >/dev/null 2>&1
    echo "$?"
}

generator_root() {
    new_root "$1"
    LIBRARIES="$WORK/$1-lib"
    mkdir -p "$LIBRARIES"
    cp "$REPO"/hooks/lib/*.sh "$LIBRARIES/"

    {
        echo "KEY=$KEY"
        echo "ESP_ROOT=$ESP_ROOT"
        echo "ESP_DIR=$ESP_DIR"
        echo "BOOTLOADER=none"
        echo "BLS=no"
    } > "$ROOT/etc/aarambh.env"

    echo "root=UUID=abc ro quiet" > "$ROOT/proc/cmdline"
    printf 'theme accordion\ntimeout 10\n' > "$ESP_DIR/aarambh.conf"
    printf '@oneshot none\nLinux known-good|known-good-id\n' > "$KERNEL_LIST"
}

leftovers() {
    ls "$ESP_DIR" | grep -c 'kernels\.' || true
}

group_kernel_list() {
    local status

    echo "== generating the kernel list"

    generator_root list-good
    boot_files vmlinuz-6.12.1 initrd.img-6.12.1
    expect_status "a good run succeeds" "$(run_generator)" 0
    case "$(sed -n 2p "$KERNEL_LIST")" in
        "Linux 6.12.1|vmlinuz-6.12.1|\\EFI\\aarambh\\linux\\test\\vmlinuz-"*"|root=UUID=abc ro quiet")
            pass "the new list replaces the old one" ;;
        *) fail "the new list replaces the old one ($(sed -n 2p "$KERNEL_LIST"))" ;;
    esac
    expect "no scratch file is left behind" "$(leftovers)" 0
    expect "the settings survive the utc_offset update" \
        "$(grep -c -e '^theme accordion$' -e '^timeout 10$' -e '^utc_offset ' "$ESP_DIR/aarambh.conf")" 3

    generator_root list-empty
    expect_status "a system without kernels is not an error" "$(run_generator)" 0
    expect "a system without kernels gets an empty list" "$(cat "$KERNEL_LIST")" "@oneshot none"

    generator_root list-fails-early
    override kernels-scan.sh 'list_scanned_kernels() { return 1; }'
    expect_status "a failed discovery is reported" "$(run_generator)" 1
    expect "a failed discovery keeps the old list" "$(sed -n 2p "$KERNEL_LIST")" \
        "Linux known-good|known-good-id"
    expect "no scratch file is left behind" "$(leftovers)" 0

    generator_root list-fails-late
    override kernels-scan.sh \
        'list_scanned_kernels() { echo "@oneshot none"; echo "Linux half|half-id"; return 1; }'
    expect_status "a discovery that fails after some output is reported" "$(run_generator)" 1
    expect "a half written list never replaces the old one" "$(sed -n 2p "$KERNEL_LIST")" \
        "Linux known-good|known-good-id"

    generator_root list-malformed
    override kernels-scan.sh 'list_scanned_kernels() { echo "Linux no-header|id"; }'
    expect_status "a list without its first line is rejected" "$(run_generator)" 1
    expect "a malformed list never replaces the old one" "$(sed -n 2p "$KERNEL_LIST")" \
        "Linux known-good|known-good-id"

    generator_root list-no-cmdline
    boot_files vmlinuz-6.12.1 initrd.img-6.12.1
    : > "$ROOT/proc/cmdline"
    expect_status "an unreadable command line is not an error" "$(run_generator)" 0
    expect "an unreadable command line means no direct boot" "$(sed -n 2p "$KERNEL_LIST")" \
        "Linux 6.12.1|vmlinuz-6.12.1"

    generator_root list-parallel
    boot_files vmlinuz-6.12.1 initrd.img-6.12.1
    run_generator > "$WORK/first.status" &
    run_generator > "$WORK/second.status" &
    wait
    status="$(cat "$WORK/first.status") $(cat "$WORK/second.status")"
    expect "two runs at the same time both succeed" "$status" "0 0"
    expect "two runs at the same time leave one complete list" "$(wc -l < "$KERNEL_LIST")" 2
    expect "two runs at the same time leave one kernel copy" "$(ls "$LINUX_DIR" | wc -l)" 2
}

group_replace_file() {
    echo "== replacing a file on the ESP"
    new_root replace-file
    echo "old" > "$ESP_DIR/file"

    echo "new" | replace_file "$ESP_DIR/file"
    expect "new content replaces the file" "$(cat "$ESP_DIR/file")" "new"

    : | replace_file "$ESP_DIR/file"
    expect_status "empty content is refused" "$?" 1
    expect "empty content keeps the file" "$(cat "$ESP_DIR/file")" "new"

    mv() { return 1; }
    echo "newer" | replace_file "$ESP_DIR/file"
    expect_status "a failed rename is reported" "$?" 1
    unset -f mv
    expect "a failed rename keeps the file" "$(cat "$ESP_DIR/file")" "new"
    expect "no scratch file is left behind" "$(ls "$ESP_DIR" | grep -c '^file')" 1
}

# A wrapper for one command, first in PATH: <name> <script body>.
wrap_command() {
    local dir="$WORK/wrap-$1"

    mkdir -p "$dir"
    printf '#!/bin/bash\n%s\n' "$2" > "$dir/$1"
    chmod +x "$dir/$1"
    echo "$dir"
}

# A copy of the published list, to compare against after a failed run.
save_list() {
    cp "$KERNEL_LIST" "$WORK/saved.kernels"
    ls "$LINUX_DIR" > "$WORK/saved.copies" 2>/dev/null || true
}

list_kept() {
    cmp -s "$KERNEL_LIST" "$WORK/saved.kernels" \
        && [ "$(ls "$LINUX_DIR" 2>/dev/null)" = "$(cat "$WORK/saved.copies")" ]
}

# Every file the list names exists on the fake ESP.
named_files_exist() {
    local path

    for path in $(awk -F'|' 'NR > 1 && NF >= 4 { print $3; n = split($4, parts, ","); for (i = 1; i <= n; i++) print parts[i] }' "$KERNEL_LIST" | tr '\\' '/'); do
        if [ ! -s "$ESP_ROOT$path" ]; then
            return 1
        fi
    done
}

listed_copy() {
    local path

    path=$(awk -F'|' -v field="$1" '$3 ~ /aarambh.linux/ { print $field; exit }' "$KERNEL_LIST" | tr '\\' '/')
    if [ -n "$path" ]; then
        echo "$ESP_ROOT$path"
    fi
}

scan_root() {
    generator_root "$1"
    boot_files vmlinuz-6.12.1 initrd.img-6.12.1
    run_generator >/dev/null
    save_list
}

bls_root() {
    generator_root "$1"
    sed -i 's/^BLS=no/BLS=yes/; s/^BOOTLOADER=none/BOOTLOADER=grub/' "$ROOT/etc/aarambh.env"
    boot_files vmlinuz-6.12.1 initramfs-6.12.1.img
    bls_entry 6.12.1 "title Linux" "version 6.12.1" "linux /vmlinuz-6.12.1" \
        "initrd /initramfs-6.12.1.img" "options root=UUID=abc ro" >/dev/null
    run_generator >/dev/null
    save_list
}

group_failed_scan() {
    local failing_sort

    echo "== a kernel scan that fails inside"
    failing_sort=$(wrap_command sort 'exit 1')

    scan_root scan-helper
    [ -n "$(listed_copy 3)" ] && pass "the first run lists the kernel with a copy" \
        || fail "the first run lists the kernel with a copy"
    override kernels-scan.sh 'boot_kernels() { return 1; }'
    expect_status "a failed boot_kernels is reported" "$(run_generator)" 1
    list_kept && pass "a failed boot_kernels keeps the list and its copies" \
        || fail "a failed boot_kernels keeps the list and its copies"

    scan_root scan-sorted
    override kernels-scan.sh 'sorted_kernels() { return 1; }'
    expect_status "the reported case: a failed sorted_kernels is reported" "$(run_generator)" 1
    list_kept && pass "a failed sorted_kernels keeps the list and its copies" \
        || fail "a failed sorted_kernels keeps the list and its copies"

    scan_root scan-sort
    expect_status "a failed sort is reported" "$(PATH="$failing_sort:$PATH" run_generator)" 1
    list_kept && pass "a failed sort keeps the list and its copies" \
        || fail "a failed sort keeps the list and its copies"

    scan_root scan-grub-ids
    override kernels-scan.sh 'grub_cfg_ids() { return 1; }'
    expect_status "unreadable GRUB menu ids are reported" "$(run_generator)" 1
    list_kept && pass "unreadable GRUB menu ids keep the list" \
        || fail "unreadable GRUB menu ids keep the list"

    bls_root scan-bls
    expect "the BLS run lists its kernel" "$(sed -n 2p "$KERNEL_LIST" | cut -d'|' -f1,2)" \
        "Linux 6.12.1|6.12.1"
    expect_status "a failed BLS listing is reported" "$(PATH="$failing_sort:$PATH" run_generator)" 1
    list_kept && pass "a failed BLS listing keeps the list" || fail "a failed BLS listing keeps the list"

    bls_root scan-bls-dirs
    override kernels-bls.sh 'bls_entry_dirs() { return 1; }'
    expect_status "failing to find the BLS folders is reported" "$(run_generator)" 1
    list_kept && pass "failing to find the BLS folders keeps the list" \
        || fail "failing to find the BLS folders keeps the list"
}

# Room for one pair of copies, never for two.
ROOM_FOR_ONE='esp_free_bytes() { if [ -n "$(ls -A "$LINUX_DIR" 2>/dev/null)" ]; then echo 1000; else echo 100000000000; fi; }'

group_low_space() {
    local failing_list_rename
    local old_initrd

    echo "== no room for a second kernel copy"
    failing_list_rename=$(wrap_command mv 'for last; do :; done
case "$last" in *.kernels) exit 1 ;; esac
exec '"$(command -v mv)"' "$@"')

    scan_root space-reclaim
    echo "new initramfs" > "$ROOT/boot/initrd.img-6.12.1"
    override direct-boot.sh "$ROOM_FOR_ONE"
    expect_status "an update with room for one pair succeeds" "$(run_generator)" 0
    if [ -n "$(listed_copy 4)" ] && cmp -s "$(listed_copy 4)" "$ROOT/boot/initrd.img-6.12.1"; then
        pass "the second pass lists the new pair with direct boot"
    else
        fail "the second pass lists the new pair with direct boot ($(sed -n 2p "$KERNEL_LIST"))"
    fi
    expect "only the new pair is left" "$(ls "$LINUX_DIR" | wc -l)" 2
    named_files_exist && pass "every file the list names exists" || fail "every file the list names exists"

    scan_root space-commit
    old_initrd=$(cat "$(listed_copy 4)")
    echo "new initramfs" > "$ROOT/boot/initrd.img-6.12.1"
    override direct-boot.sh "$ROOM_FOR_ONE"
    expect_status "the reported case: low space and a list that cannot be published fails" \
        "$(PATH="$failing_list_rename:$PATH" run_generator)" 1
    list_kept && pass "the old list and the old pair are both kept" \
        || fail "the old list and the old pair are both kept"
    named_files_exist && pass "every file the old list names still exists" \
        || fail "every file the old list names still exists"
    expect "the old initrd copy is unchanged" "$(cat "$(listed_copy 4)")" "$old_initrd"

    scan_root space-none
    echo "new initramfs" > "$ROOT/boot/initrd.img-6.12.1"
    override direct-boot.sh 'esp_free_bytes() { echo 1000; }'
    expect_status "no room even for one pair is not an error" "$(run_generator)" 0
    expect "no room: the kernel starts through the OS's loader" "$(sed -n 2p "$KERNEL_LIST")" \
        "Linux 6.12.1|vmlinuz-6.12.1"
    expect "no room: no copy is left behind" "$(ls "$LINUX_DIR" 2>/dev/null | wc -l)" 0
}

group_cache_integrity() {
    local kernel
    local before
    local efi="$REPO/build/aarambh.efi"
    local key_dir

    echo "== a kernel copy damaged on the ESP"

    scan_root cache-corrupt
    kernel=$(listed_copy 3)
    echo CORRUPT > "$kernel"
    expect_status "the reported case: a refresh over a damaged copy succeeds" "$(run_generator)" 0
    cmp -s "$(listed_copy 3)" "$ROOT/boot/vmlinuz-6.12.1" && pass "the damaged copy is written again" \
        || fail "the damaged copy is written again ($(head -c 20 "$(listed_copy 3)"))"

    head -c 3 "$ROOT/boot/vmlinuz-6.12.1" > "$(listed_copy 3)"
    run_generator >/dev/null
    cmp -s "$(listed_copy 3)" "$ROOT/boot/vmlinuz-6.12.1" && pass "a truncated copy is written again" \
        || fail "a truncated copy is written again"

    before=$(stat -c '%i %Y' "$(listed_copy 3)")
    sleep 1
    run_generator >/dev/null
    expect "an intact copy is used as it is" "$(stat -c '%i %Y' "$(listed_copy 3)")" "$before"

    if [ ! -f "$efi" ] || ! has_command sbsign || ! has_command openssl; then
        echo "  skipped: signed copies (needs build/aarambh.efi, sbsigntool and openssl)"
        return
    fi

    generator_root cache-signed
    key_dir="$WORK/cache-signed-key"
    mkdir -p "$key_dir"
    openssl req -new -x509 -newkey rsa:2048 -nodes -days 30 -subj "/CN=Aarambh test/" \
        -keyout "$key_dir/key.pem" -out "$key_dir/cert.pem" >/dev/null 2>&1
    {
        echo "SIGN_MODE=mok"
        echo "SIGN_KEY=$key_dir/key.pem"
        echo "SIGN_CERT=$key_dir/cert.pem"
    } >> "$ROOT/etc/aarambh.env"
    cp "$efi" "$ROOT/boot/vmlinuz-6.12.1"
    echo "initramfs" > "$ROOT/boot/initrd.img-6.12.1"
    run_generator >/dev/null
    kernel=$(listed_copy 3)
    sbverify --cert "$key_dir/cert.pem" "$kernel" >/dev/null 2>&1 \
        && pass "the copy is signed" || fail "the copy is signed"

    before=$(stat -c '%i %Y' "$kernel")
    sleep 1
    run_generator >/dev/null
    expect "an intact signed copy is not signed again" "$(stat -c '%i %Y' "$(listed_copy 3)")" "$before"

    printf 'X' | dd of="$kernel" bs=1 seek=4096 conv=notrunc 2>/dev/null
    run_generator >/dev/null
    sbverify --cert "$key_dir/cert.pem" "$(listed_copy 3)" >/dev/null 2>&1 \
        && pass "a signed copy with one damaged byte is signed again" \
        || fail "a signed copy with one damaged byte is signed again"
}

has_command() {
    command -v "$1" >/dev/null 2>&1
}

# A fake root for the aarambh command: Ubuntu configured, Windows detected,
# Fedora configured but its loader gone, two folders without a loader.
cli_root() {
    generator_root "$1"
    mkdir -p "$ESP_ROOT/EFI/ubuntu" "$ESP_ROOT/EFI/Microsoft/Boot" "$ESP_ROOT/EFI/diagnostics" \
        "$ESP_ROOT/EFI/oem"
    touch "$ESP_ROOT/EFI/ubuntu/shimx64.efi" "$ESP_ROOT/EFI/Microsoft/Boot/bootmgfw.efi" \
        "$ESP_ROOT/EFI/oem/tool.efi"
    cat > "$ESP_DIR/aarambh.conf" <<'CONFIG'
theme accordion
timeout 10
default 0
autodetect yes

entry Ubuntu | \EFI\ubuntu\shimx64.efi | logos/ubuntu.png | backgrounds/ubuntu.jpg | ubuntu
entry Fedora | \EFI\fedora\shimx64.efi | logos/fedora.png | backgrounds/fedora.jpg | fedora
entry Firmware | @firmware | logos/settings.png | backgrounds/settings.jpg |
CONFIG
}

run_cli() {
    AARAMBH_ROOT="$ROOT" AARAMBH_LIB_DIR="$LIBRARIES" bash "$REPO/hooks/bin/aarambh" "$@" \
        >/dev/null 2>&1
    echo "$?"
}

config_line() {
    grep "^$1 " "$ESP_DIR/aarambh.conf"
}

group_cli_default() {
    echo "== the aarambh command: default"
    cli_root cli-default

    expect_status "the reported case: an empty EFI folder is refused" "$(run_cli --default diagnostics)" 1
    expect "a refused default leaves the config as it was" "$(config_line default)" "default 0"
    expect_status "a folder whose .efi is no known loader is refused" "$(run_cli --default oem)" 1
    expect_status "a configured system whose loader is gone is refused" "$(run_cli --default fedora)" 1
    expect_status "Restart cannot be the default" "$(run_cli --default restart)" 1
    expect_status "Firmware cannot be the default" "$(run_cli --default firmware)" 1

    expect_status "a configured system with its loader is accepted" "$(run_cli --default ubuntu)" 0
    expect "it is stored by key" "$(config_line default)" "default ubuntu"
    expect_status "Windows found by its loader in any case is accepted" "$(run_cli --default Windows)" 0
    expect "it is stored by key" "$(config_line default)" "default windows"

    expect_status "index 1, the second system, is accepted" "$(run_cli --default 1)" 0
    expect_status "index 2 is past the systems and refused" "$(run_cli --default 2)" 1
    expect_status "a 30 digit index is refused" "$(run_cli --default 123456789012345678901234567890)" 1

    sed -i 's/^autodetect yes/autodetect no/' "$ESP_DIR/aarambh.conf"
    expect_status "with autodetect off, a system it would find is refused" "$(run_cli --default windows)" 1
}

group_cli_timeout() {
    echo "== the aarambh command: timeout"
    cli_root cli-timeout

    expect_status "timeout 0 is accepted" "$(run_cli --timeout 0)" 0
    expect "timeout 0 is stored" "$(config_line timeout)" "timeout 0"
    expect_status "timeout 999 is accepted" "$(run_cli --timeout 999)" 0
    expect_status "timeout 0010 is accepted" "$(run_cli --timeout 0010)" 0
    expect "it is stored without leading zeros" "$(config_line timeout)" "timeout 10"

    expect_status "timeout 1000 is refused" "$(run_cli --timeout 1000)" 1
    expect_status "the reported case: 2147483648 is refused" "$(run_cli --timeout 2147483648)" 1
    expect_status "30 digits are refused" "$(run_cli --timeout 123456789012345678901234567890)" 1
    expect_status "letters are refused" "$(run_cli --timeout 5s)" 1
    expect_status "a negative number is refused" "$(run_cli --timeout -1)" 1
    expect "refused values leave the config as it was" "$(config_line timeout)" "timeout 10"
}

group_cli_concurrent() {
    local slow_rename
    local failing_rename
    local started

    echo "== two commands changing aarambh.conf at once"
    slow_rename=$(wrap_command mv 'sleep 1
exec '"$(command -v mv)"' "$@"')
    failing_rename=$(wrap_command mv-fail 'exit 1')
    mv "$failing_rename/mv-fail" "$failing_rename/mv"

    cli_root cli-concurrent
    PATH="$slow_rename:$PATH" run_cli --theme cinema > "$WORK/theme.status" &
    PATH="$slow_rename:$PATH" run_cli --timeout 1 > "$WORK/timeout.status" &
    wait
    expect "the reported case: both commands succeed" \
        "$(cat "$WORK/theme.status") $(cat "$WORK/timeout.status")" "0 0"
    expect "both changes are kept" "$(config_line theme); $(config_line timeout)" \
        "theme cinema; timeout 1"

    cli_root cli-refresh
    boot_files vmlinuz-6.12.1 initrd.img-6.12.1
    PATH="$slow_rename:$PATH" run_cli --theme hud > "$WORK/theme.status" &
    PATH="$slow_rename:$PATH" run_generator > "$WORK/refresh.status" &
    wait
    expect "a command and a kernel refresh at once both keep their change" \
        "$(config_line theme | cut -c1-9); $(grep -c '^utc_offset ' "$ESP_DIR/aarambh.conf")" \
        "theme hud; 1"

    cli_root cli-failed-writer
    expect_status "a writer that fails is reported" "$(PATH="$failing_rename:$PATH" run_cli --theme dial)" 1
    started=$(date +%s)
    expect_status "the next writer is not blocked by it" "$(run_cli --theme dial)" 0
    [ $(( $(date +%s) - started )) -lt 5 ] && pass "the lock was released at once" \
        || fail "the lock was released at once"
}

group_missing_initrd
group_grub_variables
group_entry_owner
group_kernel_copy
group_long_values
group_kernel_list
group_replace_file
group_failed_scan
group_low_space
group_cache_integrity
group_cli_default
group_cli_timeout
group_cli_concurrent

echo
if [ "$FAILURES" = 0 ]; then
    echo "all hook failure tests passed"
else
    echo "$FAILURES hook failure test(s) failed"
    exit 1
fi
