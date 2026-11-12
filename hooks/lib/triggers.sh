# Install the refresh trigger wherever this distribution runs kernel hooks.
# Sourced by setup.sh.

install_triggers() {
    local trigger="$HOOK_DIR/triggers/aarambh-refresh"

    if [ "$BOOTLOADER" = grub ]; then
        install_file 755 "$HOOK_DIR/grub/01_aarambh" "$ROOT/etc/grub.d/01_aarambh"
    fi

    if [ "$INITRAMFS_TOOL" = initramfs-tools ]; then
        install_file 755 "$trigger" "$ROOT/etc/initramfs/post-update.d/zz-aarambh"
    fi

    if [ "$INITRAMFS_TOOL" = mkinitcpio ]; then
        install_file 755 "$trigger" "$ROOT/etc/initcpio/post/aarambh"
    fi

    if has_command kernel-install; then
        install_file 755 "$trigger" "$ROOT/etc/kernel/install.d/99-aarambh.install"
    fi

    if [ -d "$ROOT/etc/kernel/postinst.d" ]; then
        install_file 755 "$trigger" "$ROOT/etc/kernel/postinst.d/zzz-aarambh"
        install_file 755 "$trigger" "$ROOT/etc/kernel/postrm.d/zzz-aarambh"
    fi

    if [ -d "$ROOT/etc/kernel.d/post-install" ]; then
        install_file 755 "$trigger" "$ROOT/etc/kernel.d/post-install/99-aarambh"
    fi

    install_shim_triggers
}

# Copy a shim update next to Aarambh right away; on other package managers
# aarambh-sign does it at the next start.
install_shim_triggers() {
    if [ "$SIGN_MODE" != mok ]; then
        return 0
    fi

    if [ -d "$ROOT/etc/apt/apt.conf.d" ]; then
        install_file 644 "$HOOK_DIR/apt/99aarambh" "$ROOT/etc/apt/apt.conf.d/99aarambh"
    fi

    if has_command pacman; then
        install_file 644 "$HOOK_DIR/pacman/aarambh-shim.hook" "$ROOT/etc/pacman.d/hooks/aarambh-shim.hook"
    fi
}

install_clear_service() {
    if [ ! -d "$ROOT/run/systemd/system" ] || ! has_command systemctl; then
        log "no systemd: the one-time kernel choice is reset by Aarambh itself"
        return 0
    fi

    install_file 644 "$HOOK_DIR/systemd/aarambh-clear.service" \
        "$ROOT/etc/systemd/system/aarambh-clear.service"
    install_file 644 "$HOOK_DIR/systemd/aarambh-shutdown-splash.service" \
        "$ROOT/etc/systemd/system/aarambh-shutdown-splash.service"

    run systemctl daemon-reload
    run systemctl enable aarambh-clear.service aarambh-shutdown-splash.service
}
