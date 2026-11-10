# Detect the running distribution, its ESP, boot loader and initramfs tool.
# Sourced by setup.sh and install.sh.

os_release_file() {
    if [ -f "$ROOT/etc/os-release" ]; then
        echo "$ROOT/etc/os-release"
    else
        echo "$ROOT/usr/lib/os-release"
    fi
}

os_release_value() {
    (
        # shellcheck source=/dev/null
        . "$(os_release_file)"
        eval "printf '%s' \"\${$1:-}\""
    )
}

detect_os() {
    OS_ID=$(os_release_value ID | tr 'A-Z' 'a-z' | tr -cd 'a-z0-9._-')
    OS_ID_LIKE=$(os_release_value ID_LIKE | tr 'A-Z' 'a-z')
    OS_NAME=$(os_release_value NAME)

    if [ -z "$OS_ID" ]; then
        OS_ID=linux
    fi
    if [ -z "$OS_NAME" ]; then
        OS_NAME=Linux
    fi
}

# IDs to try for logos, backgrounds and themes: ID first, then ID_LIKE.
os_ids() {
    local id

    echo "$OS_ID"
    for id in $OS_ID_LIKE; do
        echo "$id"
    done
}

ESP_PARTTYPE=c12a7328-f81f-11d2-ba4b-00a0c93ec93b

is_esp_mount() {
    local dir=$1
    local fstype source parttype

    fstype=$(findmnt -no FSTYPE --mountpoint "$dir" 2>/dev/null || true)
    [ "$fstype" = vfat ] || return 1

    if [ -d "$dir/EFI" ]; then
        return 0
    fi

    # Mounted with dmask=0077 (the default on Arch), so $dir/EFI is unreadable
    # for the user running install.sh. Fall back to the partition type GUID,
    # which needs no privileges.
    source=$(findmnt -no SOURCE --mountpoint "$dir" 2>/dev/null || true)
    parttype=$(lsblk -no PARTTYPE "$source" 2>/dev/null | head -n1)
    [ "$parttype" = "$ESP_PARTTYPE" ]
}

detect_esp() {
    local dir

    if [ -n "${AARAMBH_ESP:-}" ]; then
        ESP_ROOT=$AARAMBH_ESP
        return 0
    fi

    for dir in /boot/efi /efi /boot; do
        if is_esp_mount "$dir"; then
            ESP_ROOT=$dir
            return 0
        fi
    done

    if has_command bootctl; then
        dir=$(bootctl --print-esp-path 2>/dev/null || true)
        if [ -n "$dir" ] && is_esp_mount "$dir"; then
            ESP_ROOT=$dir
            return 0
        fi
    fi

    return 1
}

detect_grub_tools() {
    GRUB_MKCONFIG=""
    GRUB_PROBE=""
    GRUB_CFG=""

    if has_command update-grub; then
        GRUB_MKCONFIG="update-grub"
    elif has_command grub2-mkconfig; then
        GRUB_MKCONFIG="grub2-mkconfig"
    elif has_command grub-mkconfig; then
        GRUB_MKCONFIG="grub-mkconfig"
    fi

    if has_command grub2-probe; then
        GRUB_PROBE="grub2-probe"
    elif has_command grub-probe; then
        GRUB_PROBE="grub-probe"
    fi

    if [ -d "$ROOT/boot/grub2" ]; then
        GRUB_CFG="/boot/grub2/grub.cfg"
    else
        GRUB_CFG="/boot/grub/grub.cfg"
    fi
}

uses_grub() {
    [ -f "$ROOT/etc/default/grub" ] && [ -n "$GRUB_MKCONFIG" ]
}

uses_systemd_boot() {
    [ -f "$ESP_ROOT/EFI/systemd/systemd-bootx64.efi" ] && {
        [ -d "$ESP_ROOT/loader/entries" ] || [ -d "$ROOT/boot/loader/entries" ]
    }
}

grub_uses_bls() {
    grep -qs '^GRUB_ENABLE_BLSCFG=true' "$ROOT/etc/default/grub"
}

detect_bootloader() {
    detect_grub_tools
    BLS=no

    if uses_grub; then
        BOOTLOADER=grub
        if grub_uses_bls; then
            BLS=yes
        fi
    elif uses_systemd_boot; then
        BOOTLOADER=systemd-boot
        BLS=yes
    else
        BOOTLOADER=other
    fi
}

detect_initramfs_tool() {
    # CachyOS (cachyos-hooks) ships an update-initramfs wrapper that only calls
    # mkinitcpio, so the name alone does not mean Debian's initramfs-tools; its
    # config directory decides.
    if has_command update-initramfs && [ -d "$ROOT/etc/initramfs-tools" ]; then
        INITRAMFS_TOOL=initramfs-tools
    elif has_command mkinitcpio; then
        INITRAMFS_TOOL=mkinitcpio
    elif has_command dracut; then
        INITRAMFS_TOOL=dracut
    elif has_command booster; then
        INITRAMFS_TOOL=booster
    else
        INITRAMFS_TOOL=none
    fi
}

filesystem_uuids() {
    findmnt -no UUID / 2>/dev/null || true
    findmnt -no UUID /boot 2>/dev/null || true
}

# The ESP folder whose grub.cfg stub points at this system's / or /boot.
grub_directory_by_uuid() {
    local uuid
    local stub

    for uuid in $(filesystem_uuids); do
        for stub in "$ESP_ROOT"/EFI/*/grub.cfg; do
            if [ -f "$stub" ] && grep -qs "$uuid" "$stub"; then
                basename "$(dirname "$stub")"
                return 0
            fi
        done
    done

    return 1
}

grub_directory_by_name() {
    local name

    for name in $(os_ids) GRUB grub; do
        if [ -d "$ESP_ROOT/EFI/$name" ]; then
            echo "$name"
            return 0
        fi
    done

    return 1
}

loader_in_directory() {
    local directory=$1
    local file

    for file in shimx64.efi shim.efi grubx64.efi; do
        if [ -f "$ESP_ROOT/EFI/$directory/$file" ]; then
            printf '\\EFI\\%s\\%s' "$directory" "$file"
            return 0
        fi
    done

    return 1
}

# UEFI path of the loader that boots this OS, e.g. \EFI\ubuntu\shimx64.efi
detect_loader() {
    local directory

    if [ -n "${LOADER_OVERRIDE:-}" ]; then
        echo "$LOADER_OVERRIDE"
        return 0
    fi

    case "$BOOTLOADER" in
        systemd-boot)
            printf '\\EFI\\systemd\\systemd-bootx64.efi'
            return 0
            ;;
        grub)
            directory=$(grub_directory_by_uuid || grub_directory_by_name || true)
            if [ -n "$directory" ]; then
                loader_in_directory "$directory"
                return
            fi
            ;;
    esac

    return 1
}
