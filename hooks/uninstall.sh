#!/bin/bash
#
# Disconnect the running Linux installation from Aarambh and restore every
# file setup.sh changed. With --purge it also removes Aarambh from the EFI
# partition, deletes its firmware boot entry and its own Secure Boot key (do
# this on the last OS).
#
# Usage: uninstall.sh [--dry-run] [--purge]

set -euo pipefail

HOOK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRY_RUN=0
PURGE=0

for argument in "$@"; do
    case "$argument" in
        --dry-run) DRY_RUN=1 ;;
        --purge) PURGE=1 ;;
        *) sed -n '3,9p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 1 ;;
    esac
done

if [ "$(id -u)" != 0 ] && [ "$DRY_RUN" = 0 ] && [ -z "${AARAMBH_ROOT:-}" ]; then
    exec sudo bash "${BASH_SOURCE[0]}" "$@"
fi

for library in common detect manifest grub-config initramfs signing; do
    # shellcheck source=/dev/null
    . "$HOOK_DIR/lib/$library.sh"
done

remove_menu_entry() {
    local config="$ESP_DIR/aarambh.conf"

    if [ ! -f "$config" ]; then
        return 0
    fi

    if [ "$DRY_RUN" = 1 ]; then
        echo "  would remove the '$KEY' entry from $config"
        return 0
    fi

    ENTRY_KEY="$KEY" awk '
        BEGIN { key = ENVIRON["ENTRY_KEY"] }
        /^entry[ \t]/ {
            split($0, fields, "|")
            gsub(/^[ \t]+|[ \t]+$/, "", fields[5])
            if (fields[5] == key) {
                next
            }
        }
        { print }
    ' "$config" | replace_file "$config" \
        || warn "could not remove the '$KEY' entry from $config"
}

remove_esp_files() {
    run rm -rf "$ESP_DIR/linux/$KEY"
    run rm -f "$ESP_DIR/$KEY.kernels" "$ESP_DIR/next-$KEY.cfg"
    run rm -f "$ESP_DIR/shutdown/$KEY.stamp" "$ESP_DIR/shutdown/$KEY-bg.png" \
        "$ESP_DIR/shutdown/$KEY-reboot.png" "$ESP_DIR/shutdown/$KEY-shutdown.png" \
        "$ESP_DIR/shutdown/$KEY-dot.png"
}

undo_manifest() {
    local kind
    local path

    if [ ! -f "$MANIFEST" ]; then
        warn "no manifest found at $MANIFEST, nothing to restore"
        return 0
    fi

    if [ -f "$ROOT/etc/systemd/system/aarambh-clear.service" ] && has_command systemctl; then
        run systemctl disable aarambh-clear.service
    fi

    if [ -f "$ROOT/etc/systemd/system/aarambh-shutdown-splash.service" ] && has_command systemctl; then
        run systemctl disable aarambh-shutdown-splash.service
    fi

    tac "$MANIFEST" | while read -r kind path; do
        case "$kind" in
            file) run rm -f "$path" ;;
            directory) run rm -rf "$path" ;;
            alternative) run update-alternatives --remove default.plymouth "$path" ;;
            backup) run mv -f "$path$BACKUP_SUFFIX" "$path" ;;
        esac
    done

    run rm -rf "$MANIFEST_DIR"
    run rmdir --ignore-fail-on-non-empty "$ROOT/usr/local/lib/aarambh" 2>/dev/null || true

    if has_command systemctl && [ -d "$ROOT/run/systemd/system" ]; then
        run systemctl daemon-reload
    fi
}

remove_boot_entry() {
    local number

    number=$(efibootmgr 2>/dev/null | sed -n 's/^Boot\([0-9A-Fa-f]\{4\}\)\*\{0,1\} Aarambh.*/\1/p' | head -n1)

    if [ -n "$number" ]; then
        run efibootmgr -q -b "$number" -B
    fi
}

load_environment || die "Aarambh is not set up on this system ($ENV_FILE missing)"
detect_os
detect_bootloader
detect_initramfs_tool

log "removing Aarambh from $OS_NAME"

remove_menu_entry
remove_esp_files
undo_manifest
rebuild_initramfs
regenerate_grub

if [ "$PURGE" = 1 ]; then
    remove_boot_entry
    forget_own_key
    run rm -rf "$ESP_DIR"
    log "Aarambh removed from the EFI partition"
fi

log "done. Your firmware will boot the next entry in BootOrder."