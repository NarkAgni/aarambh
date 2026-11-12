# Install the Aarambh Plymouth theme for the running distribution. Sourced.
# Returns success when the initramfs has to be rebuilt.

PLYMOUTH_THEME_DIR="$ROOT/usr/share/plymouth/themes/aarambh"
PLYMOUTHD_CONF="$ROOT/etc/plymouth/plymouthd.conf"

plymouth_theme_source() {
    if [ -f "$ESP_DIR/plymouth/$OS_ID/aarambh.plymouth" ]; then
        echo "$ESP_DIR/plymouth/$OS_ID"
        return 0
    fi

    return 1
}

# One path per layout: /usr/lib64 (Fedora, openSUSE), /usr/lib (Arch, Void)
# and /usr/lib/<multiarch> (Debian, Ubuntu). Each is tested on its own, as a
# single ls over all of them fails as soon as one pattern matches nothing.
has_script_plugin() {
    local plugin

    for plugin in "$ROOT"/usr/lib*/plymouth/script.so "$ROOT"/usr/lib/*/plymouth/script.so; do
        if [ -f "$plugin" ]; then
            return 0
        fi
    done

    return 1
}

install_script_plugin() {
    if has_command dnf; then
        run dnf install -y plymouth-plugin-script
    elif has_command zypper; then
        run zypper --non-interactive install plymouth-plugin-script
    else
        return 1
    fi
}

uses_debian_alternatives() {
    has_command update-alternatives \
        && update-alternatives --query default.plymouth >/dev/null 2>&1
}

select_theme() {
    local theme_file="$PLYMOUTH_THEME_DIR/aarambh.plymouth"

    if uses_debian_alternatives; then
        run update-alternatives --install /usr/share/plymouth/themes/default.plymouth \
            default.plymouth "$theme_file" 200
        run update-alternatives --set default.plymouth "$theme_file"
        manifest_add alternative "$theme_file"
    elif has_command plymouth-set-default-theme; then
        run plymouth-set-default-theme aarambh
    else
        warn "could not select the Plymouth theme, set 'aarambh' as default yourself"
    fi
}

write_plymouthd_conf() {
    backup_once "$PLYMOUTHD_CONF"
    manifest_add file "$PLYMOUTHD_CONF"

    printf '[Daemon]\nTheme=aarambh\nShowDelay=0\nDeviceTimeout=8\n' \
        | write_file "$PLYMOUTHD_CONF"
}

check_mkinitcpio_hook() {
    if [ "$INITRAMFS_TOOL" != mkinitcpio ]; then
        return 0
    fi

    if ! grep -qs '^HOOKS=.*plymouth' "$ROOT/etc/mkinitcpio.conf" "$ROOT"/etc/mkinitcpio.conf.d/*.conf; then
        warn "add 'plymouth' to HOOKS in /etc/mkinitcpio.conf and 'splash' to the kernel"
        warn "command line, otherwise the splash will not show (see the README)"
    fi
}

# The restart and shutdown frames Aarambh drew for the current theme live on
# the ESP; aarambh-clear copies them in at every boot. Reinstalling the theme
# wipes the folder, so put them back now, or the very next restart shows the
# plain splash instead of the chosen theme's.
restore_shutdown_frames() {
    local part

    for part in bg reboot shutdown dot; do
        if [ ! -f "$ESP_DIR/shutdown/$KEY-$part.png" ]; then
            return 0
        fi
    done

    for part in bg reboot shutdown dot; do
        run cp "$ESP_DIR/shutdown/$KEY-$part.png" "$PLYMOUTH_THEME_DIR/still-$part.png"
    done
}

install_plymouth_theme() {
    local source

    if [ "${SKIP_PLYMOUTH:-0}" = 1 ]; then
        log "skipping Plymouth (--no-plymouth)"
        return 1
    fi

    if ! has_command plymouthd; then
        log "Plymouth is not installed, skipping the boot splash"
        return 1
    fi

    source=$(plymouth_theme_source || true)
    if [ -z "$source" ]; then
        log "no Plymouth theme for '$OS_ID' on the ESP, skipping the boot splash"
        return 1
    fi

    if ! has_script_plugin && ! install_script_plugin; then
        warn "the Plymouth script plugin is missing, skipping the boot splash"
        return 1
    fi

    run rm -rf "$PLYMOUTH_THEME_DIR"
    run mkdir -p "$PLYMOUTH_THEME_DIR"
    run cp -r "$source/." "$PLYMOUTH_THEME_DIR/"
    restore_shutdown_frames
    manifest_add directory "$PLYMOUTH_THEME_DIR"

    write_plymouthd_conf
    select_theme
    check_mkinitcpio_hook
    return 0
}
