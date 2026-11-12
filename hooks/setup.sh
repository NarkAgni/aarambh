#!/bin/bash
#
# Connect the running Linux installation to Aarambh.
#
# Detects the distribution, boot loader and initramfs generator, installs the
# kernel-list generator with the matching hooks, adds this OS to the menu,
# makes GRUB silent and installs the Plymouth theme. Run it once on every
# Linux installation that shares the EFI partition.
#
# Usage: setup.sh [--dry-run] [--no-plymouth] [--loader '\EFI\<dir>\<file>.efi']

set -euo pipefail

HOOK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIB_DIR=/usr/local/lib/aarambh
BIN_DIR=/usr/local/sbin
COMMAND_DIR=/usr/local/bin
LIB_FILES="common.sh cmdline.sh direct-boot.sh kernels-bls.sh kernels-scan.sh signing.sh"

DRY_RUN=0
SKIP_PLYMOUTH=0
LOADER_OVERRIDE=""

usage() {
    sed -n '3,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

parse_arguments() {
    while [ "$#" -gt 0 ]; do
        case "$1" in
            --dry-run) DRY_RUN=1 ;;
            --no-plymouth) SKIP_PLYMOUTH=1 ;;
            --loader)
                shift
                LOADER_OVERRIDE="${1:-}"
                ;;
            -h|--help) usage 0 ;;
            *) usage 1 ;;
        esac
        shift
    done
}

parse_arguments "$@"

if [ "$(id -u)" != 0 ] && [ "$DRY_RUN" = 0 ] && [ -z "${AARAMBH_ROOT:-}" ]; then
    exec sudo bash "${BASH_SOURCE[0]}" "$@"
fi

for library in common detect manifest triggers grub-config initramfs plymouth menu-entry signing; do
    # shellcheck source=/dev/null
    . "$HOOK_DIR/lib/$library.sh"
done

locate_esp() {
    case "$HOOK_DIR" in
        */EFI/aarambh/hooks)
            ESP_ROOT="${HOOK_DIR%/EFI/aarambh/hooks}"
            ;;
        *)
            detect_esp || die "no EFI system partition found at /boot/efi, /efi or /boot"
            ;;
    esac

    ESP_DIR="$ESP_ROOT/EFI/aarambh"

    if [ ! -d "$ESP_DIR" ]; then
        if [ "$DRY_RUN" = 1 ]; then
            warn "$ESP_DIR does not exist yet, it is created by scripts/install.sh"
        else
            die "$ESP_DIR not found. Run scripts/install.sh first."
        fi
    fi
}

print_detection() {
    log "distribution:    $OS_NAME ($OS_ID${OS_ID_LIKE:+, like $OS_ID_LIKE})"
    log "EFI partition:   $ESP_ROOT"
    log "boot loader:     $BOOTLOADER${BLS:+ (BLS: $BLS)}"
    log "initramfs:       $INITRAMFS_TOOL"
    log "loader path:     $LOADER"
    log "secure boot:     $(secure_boot_state)"
}

secure_boot_state() {
    local state=off

    if secure_boot_on; then
        state=on
    fi

    case "$SIGN_MODE" in
        mok) echo "$state, signed with $SIGN_DER, started by $SHIM_DIR/$SHIM_FILE" ;;
        db) echo "$state, signed with the sbctl db key" ;;
        *) echo "$state, not signed (no shim on the EFI partition)" ;;
    esac
}

warn_unsigned() {
    if [ "$SIGN_MODE" = none ] && secure_boot_on; then
        warn "Secure Boot is on, but no shim was found on the EFI partition, so Aarambh"
        warn "cannot be signed. Install your distribution's shim package (shim-signed on"
        warn "Ubuntu and Debian, shim-x64 on Fedora, shim on openSUSE) and run setup.sh again."
    fi
}

setup_signed_boot() {
    write_signing_cert
    sync_shim
    install_signed_loader
}

write_environment() {
    {
        echo "KEY=$KEY"
        echo "ESP_ROOT=$ESP_ROOT"
        echo "ESP_DIR=$ESP_DIR"
        echo "BOOTLOADER=$BOOTLOADER"
        echo "BLS=$BLS"
        echo "GRUB_PROBE=$GRUB_PROBE"
        echo "GRUB_CFG=$GRUB_CFG"
        signing_environment
    } | write_file "$ENV_FILE"

    manifest_add file "$ENV_FILE"
}

install_scripts() {
    local file

    install_file 755 "$HOOK_DIR/bin/aarambh-kernels" "$ROOT$BIN_DIR/aarambh-kernels"
    install_file 755 "$HOOK_DIR/bin/aarambh-clear" "$ROOT$BIN_DIR/aarambh-clear"
    install_file 755 "$HOOK_DIR/bin/aarambh-sign" "$ROOT$BIN_DIR/aarambh-sign"
    install_file 755 "$HOOK_DIR/bin/aarambh" "$ROOT$COMMAND_DIR/aarambh"
    install_file 755 "$HOOK_DIR/bin/aarambh-debug" "$ROOT$COMMAND_DIR/aarambh-debug"

    run rm -f "$ROOT$BIN_DIR/aarambh" "$ROOT$LIB_DIR/ubuntu.sh" "$ROOT$LIB_DIR/fedora.sh"

    for file in $LIB_FILES; do
        install_file 644 "$HOOK_DIR/lib/$file" "$ROOT$LIB_DIR/$file"
    done
}

reset_oneshot() {
    if [ "$DRY_RUN" = 0 ]; then
        : > "$ESP_DIR/next-$KEY.cfg"
    fi
}

refresh_kernel_list() {
    if [ "$DRY_RUN" = 1 ]; then
        echo "  would run: $BIN_DIR/aarambh-kernels"
        return 0
    fi

    AARAMBH_LIB_DIR="$ROOT$LIB_DIR" bash "$ROOT$BIN_DIR/aarambh-kernels" \
        || warn "the kernel list was not refreshed; $OS_NAME starts through its own boot loader until 'sudo aarambh-kernels' works"
}

print_summary() {
    local list="$ESP_DIR/$KEY.kernels"

    echo
    log "done: $OS_NAME is connected to Aarambh"

    if [ -f "$list" ]; then
        grep -v '^@' "$list" | cut -d'|' -f1,3 | sed 's/|\(.\+\)$/   [direct boot]/; s/|$//' \
            | sed 's/^/    /'
    fi

    df -h "$ESP_ROOT" 2>/dev/null | tail -n1 | awk '{print "aarambh: EFI partition free: " $4}'
    log "change theme or default OS with: aarambh --theme <name> --default <os>"
    log "if anything goes wrong, run aarambh-debug and attach the report to an issue"
}

main() {
    local rebuild=0

    detect_os
    KEY=$OS_ID
    locate_esp
    detect_bootloader
    detect_initramfs_tool

    LOADER=$(detect_loader || true)
    if [ -z "$LOADER" ]; then
        die "could not find the EFI loader of $OS_NAME. Pass it with --loader '\\EFI\\<dir>\\<file>.efi'"
    fi

    detect_signing || true
    print_detection
    warn_unsigned

    if [ "$DRY_RUN" = 1 ]; then
        log "dry run: nothing will be changed"
    fi

    write_environment
    install_scripts
    install_triggers
    install_clear_service
    setup_signed_boot
    reset_oneshot
    register_menu_entry "$LOADER"
    configure_grub

    if tune_initramfs; then
        rebuild=1
    fi

    if install_plymouth_theme; then
        rebuild=1
    fi

    if [ "$rebuild" = 1 ]; then
        rebuild_initramfs
    fi

    regenerate_grub
    refresh_kernel_list
    queue_enrollment
    print_summary
    print_enrollment_steps
}

main