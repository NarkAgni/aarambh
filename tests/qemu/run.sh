#!/bin/bash
#
# Boot Aarambh in QEMU from a throwaway ESP image.
#
# The image contains fake loaders (copies of aarambh.efi) for Ubuntu and
# Fedora, which are listed in fixtures/aarambh.conf 
# which Aarambh finds on its own through autodetect.
#
# Usage: tests/qemu/run.sh [--no-kvm] [--secure-boot] [--second-esp] [--theme <name>] [--default <os>]
#
#   --theme        Theme to show, e.g. cinema (default: the one in the fixture)
#   --default      Entry selected at start: ubuntu, fedora a title or an index
#   --secure-boot  Boot like a real PC with Secure Boot on: Microsoft keys,
#                  the distribution's shim, Aarambh signed with a throwaway
#                  key that is already enrolled in MOK
#   --second-esp   Add a second ESP holding only Windows, the layout you get
#                  when Windows was installed first and Linux brought its own

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEST_DIR="$ROOT/tests/qemu"
WORK_DIR="$ROOT/build/qemu"
IMAGE="$WORK_DIR/esp.img"
WINDOWS_IMAGE="$WORK_DIR/esp-windows.img"
VARS="$WORK_DIR/vars.fd"
OVMF_DIRS="${OVMF_DIR:-} /usr/share/OVMF /usr/share/edk2/x64 /usr/share/edk2/ovmf /usr/share/edk2-ovmf/x64"
ESP_DIR="::/EFI/aarambh"
CONFIG="$WORK_DIR/aarambh.conf"
THEMES="accordion minimal glass-sidebar editorial editorial-dark cinema honeycomb constellation picker-wheel dial hud departures gallery"

export MTOOLS_SKIP_CHECK=1

ACCEL=(-enable-kvm -cpu host)
THEME=""
DEFAULT=""
SECURE_BOOT=0
SECOND_ESP=0
KEY_DIR="$WORK_DIR/keys"
SHIM_LOCK_GUID=605dab50-e046-4300-abb6-3dd810dd8b23

usage() {
    sed -n '3,19p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --no-kvm) ACCEL=() ;;
        --secure-boot) SECURE_BOOT=1 ;;
        --second-esp) SECOND_ESP=1 ;;
        --theme)
            THEME="${2:-}"
            shift
            ;;
        --default)
            DEFAULT="${2:-}"
            shift
            ;;
        -h|--help) usage 0 ;;
        *) usage 1 ;;
    esac
    shift
done

if [ -n "$THEME" ] && [[ " $THEMES " != *" $THEME "* ]]; then
    echo "Unknown theme '$THEME'. Available: $THEMES" >&2
    exit 1
fi

require() {
    if ! command -v "$1" >/dev/null; then
        echo "Missing '$1'. Install it with: sudo apt install $2" >&2
        exit 1
    fi
}

# find_firmware <name...>: the first one present, under any known name and
# directory; distributions disagree about both.
find_firmware() {
    local dir
    local name

    for dir in $OVMF_DIRS; do
        for name in "$@"; do
            if [ -f "$dir/$name" ]; then
                echo "$dir/$name"
                return 0
            fi
        done
    done

    return 1
}

need_firmware() {
    local file

    if ! file=$(find_firmware "$@"); then
        echo "Missing OVMF firmware ($1). Install edk2-ovmf (Arch), ovmf (Debian)" >&2
        echo "or edk2-ovmf (Fedora), or point OVMF_DIR at the directory." >&2
        exit 1
    fi

    echo "$file"
}

build() {
    make -C "$ROOT"

    if [ ! -d "$ROOT/assets/fonts" ]; then
        "$ROOT/scripts/make-assets.sh"
    fi
}

create_image() {
    mkdir -p "$WORK_DIR"
    rm -f "$IMAGE"

    dd if=/dev/zero of="$IMAGE" bs=1M count=64 status=none
    mkfs.vfat -F 32 "$IMAGE" >/dev/null

    mmd -i "$IMAGE" ::/EFI ::/EFI/BOOT ::/EFI/ubuntu ::/EFI/fedora "$ESP_DIR"

    if [ "$SECOND_ESP" = 1 ]; then
        rm -f "$WINDOWS_IMAGE"
        dd if=/dev/zero of="$WINDOWS_IMAGE" bs=1M count=260 status=none
        mkfs.vfat -F 32 "$WINDOWS_IMAGE" >/dev/null
        mmd -i "$WINDOWS_IMAGE" ::/EFI ::/EFI/Microsoft ::/EFI/Microsoft/Boot
    fi
}

write_config() {
    cp "$TEST_DIR/fixtures/aarambh.conf" "$CONFIG"

    if [ -n "$THEME" ]; then
        sed -i "s/^theme .*/theme $THEME/" "$CONFIG"
    fi

    if [ -n "$DEFAULT" ]; then
        sed -i "s/^default .*/default $DEFAULT/" "$CONFIG"
    fi

    grep -E '^(theme|default) ' "$CONFIG"
}

# find_file <candidate...>: the first one that exists (the ESP may need root).
find_file() {
    local file

    for file in "$@"; do
        if [ -f "$file" ] || sudo test -f "$file" 2>/dev/null; then
            echo "$file"
            return 0
        fi
    done

    return 1
}

read_file() {
    if [ -r "$1" ]; then
        cat "$1"
    else
        sudo cat "$1"
    fi
}

prepare_secure_boot() {
    local shim
    local manager

    shim=${SHIM:-$(find_file /usr/lib/shim/shimx64.efi.signed.latest /usr/lib/shim/shimx64.efi.signed \
        /boot/efi/EFI/*/shimx64.efi /efi/EFI/*/shimx64.efi)} \
        || { echo "No signed shim found. Install shim-signed (Ubuntu, Debian) or shim-x64 (Fedora), or pass SHIM=<file>" >&2; exit 1; }
    manager=${MOK_MANAGER:-$(find_file /usr/lib/shim/mmx64.efi.signed /usr/lib/shim/mmx64.efi \
        "$(dirname "$shim")/mmx64.efi")} \
        || { echo "No MokManager (mmx64.efi) found next to $shim, pass MOK_MANAGER=<file>" >&2; exit 1; }

    mkdir -p "$KEY_DIR"
    if [ ! -f "$KEY_DIR/test.key" ]; then
        openssl req -new -x509 -newkey rsa:2048 -nodes -sha256 -days 3650 -subj "/CN=Aarambh QEMU test/" \
            -keyout "$KEY_DIR/test.key" -out "$KEY_DIR/test.crt" 2>/dev/null
    fi

    sbsign --key "$KEY_DIR/test.key" --cert "$KEY_DIR/test.crt" \
        --output "$WORK_DIR/aarambh.signed.efi" "$ROOT/build/aarambh.efi" 2>/dev/null
    read_file "$shim" > "$WORK_DIR/shimx64.efi"
    read_file "$manager" > "$WORK_DIR/mmx64.efi"
}

copy_boot_files() {
    local efi=$1

    if [ "$SECURE_BOOT" = 1 ]; then
        mcopy -i "$IMAGE" "$WORK_DIR/shimx64.efi" ::/EFI/BOOT/BOOTX64.EFI
        mcopy -i "$IMAGE" "$WORK_DIR/mmx64.efi" ::/EFI/BOOT/mmx64.efi
        mcopy -i "$IMAGE" "$efi" ::/EFI/BOOT/grubx64.efi
    else
        mcopy -i "$IMAGE" "$efi" ::/EFI/BOOT/BOOTX64.EFI
    fi
}

copy_files() {
    local efi="$ROOT/build/aarambh.efi"
    local folder

    if [ "$SECURE_BOOT" = 1 ]; then
        efi="$WORK_DIR/aarambh.signed.efi"
    fi

    copy_boot_files "$efi"
    mcopy -i "$IMAGE" "$efi" ::/EFI/ubuntu/shimx64.efi
    mcopy -i "$IMAGE" "$efi" ::/EFI/fedora/shimx64.efi
    # mcopy -i "$IMAGE" "$efi" ::/EFI/debian/grubx64.efi
    # mcopy -i "$IMAGE" "$efi" ::/EFI/Microsoft/Boot/bootmgfw.efi

    if [ "$SECOND_ESP" = 1 ]; then
        mcopy -i "$WINDOWS_IMAGE" "$efi" ::/EFI/Microsoft/Boot/bootmgfw.efi
    fi

    mcopy -i "$IMAGE" "$CONFIG" "$ESP_DIR/"
    mcopy -i "$IMAGE" "$TEST_DIR/fixtures/ubuntu.kernels" "$ESP_DIR/"

    for folder in fonts logos backgrounds; do
        mcopy -s -i "$IMAGE" "$ROOT/assets/$folder" "$ESP_DIR/"
    done
}

run_qemu() {
    local code
    local machine=q35
    local secure=()
    local windows=()

    code=$(need_firmware OVMF_CODE_4M.fd OVMF_CODE.4m.fd OVMF_CODE.fd)
    cp "$(need_firmware OVMF_VARS_4M.fd OVMF_VARS.4m.fd OVMF_VARS.fd)" "$VARS"

    if [ "$SECURE_BOOT" = 1 ]; then
        code=$(need_firmware OVMF_CODE_4M.secboot.fd OVMF_CODE.secboot.4m.fd \
                             OVMF_CODE.secboot.fd)
        machine=q35,smm=on
        # shellcheck disable=SC2054
        secure=(-global driver=cfi.pflash01,property=secure,value=on)
        virt-fw-vars -i "$(need_firmware OVMF_VARS_4M.ms.fd OVMF_VARS.ms.4m.fd)" -o "$VARS" \
            --add-mok "$SHIM_LOCK_GUID" "$KEY_DIR/test.crt" >/dev/null
    fi

    if [ "$SECOND_ESP" = 1 ]; then
        windows=(-drive format=raw,file="$WINDOWS_IMAGE")
    fi

    qemu-system-x86_64 "${ACCEL[@]}" \
        -machine "$machine" \
        "${secure[@]}" \
        -m 1024 \
        -drive if=pflash,format=raw,readonly=on,file="$code" \
        -drive if=pflash,format=raw,file="$VARS" \
        -drive format=raw,file="$IMAGE" \
        "${windows[@]}" \
        -net none \
        ${QEMU_DISPLAY:--display gtk}
}

require qemu-system-x86_64 qemu-system-x86
require mcopy mtools
require mkfs.vfat dosfstools

build

if [ "$SECURE_BOOT" = 1 ]; then
    require sbsign sbsigntool
    require openssl openssl
    require virt-fw-vars python3-virt-firmware
    prepare_secure_boot
fi

create_image
write_config
copy_files
run_qemu