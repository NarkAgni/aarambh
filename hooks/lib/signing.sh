# Secure Boot support. Sourced.
#
# Aarambh starts through the distribution's own shim, which Microsoft signs
# and every PC trusts. Shim starts \EFI\aarambh\<second stage>, which is
# Aarambh signed with a Machine Owner Key (MOK), and Aarambh asks shim to check
# every kernel and loader it starts. Kernels copied for direct boot are signed
# with the same key. Without shim, keys enrolled with sbctl are used instead.

SECURE_BOOT_VAR=$ROOT/sys/firmware/efi/efivars/SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c
KEY_DIR="$ROOT/var/lib/aarambh/keys"
SBCTL_DB="$ROOT/var/lib/sbctl/keys/db"
PASSWORD_FILE="$KEY_DIR/mok-password"
MOK_QUEUED=0

# The variable is read through a pipe: some kernels refuse seeking in
# efivarfs, and od would then print nothing. mokutil is asked when the
# variable cannot be read at all.
secure_boot_on() {
    local value

    value=$(cat "$SECURE_BOOT_VAR" 2>/dev/null | od -An -t u1 -j4 -N1 2>/dev/null | tr -d ' ')

    if [ -n "$value" ]; then
        [ "$value" = 1 ]
        return
    fi

    has_command mokutil && mokutil --sb-state 2>/dev/null | grep -q 'SecureBoot enabled'
}

signing_enabled() {
    [ "${SIGN_MODE:-none}" != none ] && [ -f "${SIGN_KEY:-}" ] && [ -f "${SIGN_CERT:-}" ]
}

# sign_file <input> <output>
sign_file() {
    sbsign --key "$SIGN_KEY" --cert "$SIGN_CERT" --output "$2" "$1" >/dev/null 2>&1 \
        && sbverify --cert "$SIGN_CERT" "$2" >/dev/null 2>&1
}

signed_by_us() {
    sbverify --cert "$SIGN_CERT" "$1" >/dev/null 2>&1
}

has_signature() {
    sbverify --list "$1" 2>/dev/null | grep -q '^signature '
}

# Identifies the key, so copies are signed again when the key changes.
signing_id() {
    if signing_enabled; then
        sha256sum < "$SIGN_CERT" | cut -d' ' -f1
    else
        echo unsigned
    fi
}

# enrolled, pending or absent
mok_state() {
    local output

    output=$(mokutil --test-key "$1" 2>&1 || true)

    case "$output" in
        *"already enrolled"*) echo enrolled ;;
        *"enrollment request"*) echo pending ;;
        *) echo absent ;;
    esac
}

# Shim refuses images signed with a key whose extended key usage marks it for
# kernel modules (1.3.6.1.4.1.2312.16.1.2), as the MOK keys Ubuntu and DKMS
# create for drivers are. Such a key is never used for Aarambh.
can_sign_loaders() {
    ! openssl x509 -inform der -in "$1" -noout -ext extendedKeyUsage 2>/dev/null \
        | grep -q '1\.3\.6\.1\.4\.1\.2312\.16\.1\.2'
}

# Existing keys first, so a key the PC already trusts for DKMS or akmods
# modules is reused and no enrollment is needed, unless it is marked for
# kernel modules only (see can_sign_loaders): <private key>|<DER cert>
mok_key_candidates() {
    printf '%s\n' \
        "$ROOT/var/lib/shim-signed/mok/MOK.priv|$ROOT/var/lib/shim-signed/mok/MOK.der" \
        "$ROOT/etc/pki/akmods/private/private_key.priv|$ROOT/etc/pki/akmods/certs/public_key.der" \
        "$ROOT/var/lib/dkms/mok.key|$ROOT/var/lib/dkms/mok.pub" \
        "$KEY_DIR/aarambh.key|$KEY_DIR/aarambh.der"
}

generate_key() {
    log "creating a Secure Boot signing key in $KEY_DIR"
    run mkdir -p "$KEY_DIR"
    run chmod 700 "$KEY_DIR"
    run openssl req -new -x509 -newkey rsa:2048 -nodes -sha256 -days 36500 \
        -subj "/CN=Aarambh Secure Boot ($(uname -n))/" \
        -addext "extendedKeyUsage=codeSigning" \
        -keyout "$KEY_DIR/aarambh.key" -out "$KEY_DIR/aarambh.crt" 2>/dev/null
    run openssl x509 -in "$KEY_DIR/aarambh.crt" -outform der -out "$KEY_DIR/aarambh.der"
    run chmod 600 "$KEY_DIR/aarambh.key"
}

choose_mok_key() {
    local key
    local der
    local state
    local pending=""
    local existing=""

    while IFS='|' read -r key der; do
        if [ ! -f "$key" ] || [ ! -f "$der" ] || ! can_sign_loaders "$der"; then
            continue
        fi

        state=$(mok_state "$der")
        if [ "$state" = enrolled ]; then
            printf '%s|%s' "$key" "$der"
            return 0
        fi

        if [ "$state" = pending ] && [ -z "$pending" ]; then
            pending="$key|$der"
        elif [ -z "$existing" ]; then
            existing="$key|$der"
        fi
    done < <(mok_key_candidates)

    if [ -n "$pending" ]; then
        printf '%s' "$pending"
    elif [ -n "$existing" ]; then
        printf '%s' "$existing"
    else
        generate_key >&2
        printf '%s|%s' "$KEY_DIR/aarambh.key" "$KEY_DIR/aarambh.der"
    fi
}

# <shim>|<MokManager>|<file shim starts>, by the names the distribution uses.
shim_layout() {
    if [ -f "$1/shimx64.efi" ] && [ -f "$1/mmx64.efi" ]; then
        echo "shimx64.efi|mmx64.efi|grubx64.efi"
    elif [ -f "$1/shim.efi" ] && [ -f "$1/MokManager.efi" ]; then
        echo "shim.efi|MokManager.efi|grub.efi"
    else
        return 1
    fi
}

# This OS's shim first, then any other distribution's, then the copy that
# another OS already put into Aarambh's folder.
find_shim_dir() {
    local id
    local dir

    for id in $(os_ids); do
        if shim_layout "$ESP_ROOT/EFI/$id" >/dev/null; then
            echo "$ESP_ROOT/EFI/$id"
            return 0
        fi
    done

    for dir in "$ESP_ROOT"/EFI/*/; do
        dir=${dir%/}
        case "${dir##*/}" in
            aarambh|BOOT|boot|Boot) continue ;;
        esac

        if shim_layout "$dir" >/dev/null; then
            echo "$dir"
            return 0
        fi
    done

    if shim_layout "$ESP_DIR" >/dev/null; then
        echo "$ESP_DIR"
        return 0
    fi

    return 1
}

signing_install_command() {
    if has_command apt-get; then
        echo "apt-get install -y sbsigntool mokutil"
    elif has_command dnf; then
        echo "dnf install -y sbsigntools mokutil"
    elif has_command zypper; then
        echo "zypper --non-interactive install sbsigntools mokutil"
    elif has_command pacman; then
        echo "pacman -S --needed --noconfirm sbsigntools mokutil"
    elif has_command xbps-install; then
        echo "xbps-install -y sbsigntool mokutil"
    fi
}

ensure_signing_tools() {
    local command

    if has_command sbsign && has_command sbverify && has_command mokutil && has_command openssl; then
        return 0
    fi

    command=$(signing_install_command)
    if [ -z "$command" ]; then
        return 1
    fi

    log "installing the Secure Boot signing tools: $command"
    # shellcheck disable=SC2086
    run $command || return 1

    if [ "$DRY_RUN" = 1 ]; then
        return 0
    fi

    has_command sbsign && has_command sbverify && has_command mokutil && has_command openssl
}

detect_signing() {
    local layout
    local pair

    SIGN_MODE=none
    SIGN_KEY=""
    SIGN_CERT=""
    SIGN_DER=""
    SHIM_DIR=""
    SHIM_FILE=""
    SHIM_MANAGER=""
    SHIM_SECOND=""

    if SHIM_DIR=$(find_shim_dir); then
        if ! ensure_signing_tools; then
            warn "sbsigntools and mokutil could not be installed: $(signing_install_command)"
            SHIM_DIR=""
            return 1
        fi

        layout=$(shim_layout "$SHIM_DIR")
        IFS='|' read -r SHIM_FILE SHIM_MANAGER SHIM_SECOND <<< "$layout"
        pair=$(choose_mok_key)
        IFS='|' read -r SIGN_KEY SIGN_DER <<< "$pair"
        SIGN_CERT="$KEY_DIR/signing.crt"
        SIGN_MODE=mok
    elif [ -f "$SBCTL_DB/db.key" ] && [ -f "$SBCTL_DB/db.pem" ]; then
        if ! ensure_signing_tools; then
            warn "sbsigntools could not be installed: $(signing_install_command)"
            return 1
        fi

        SIGN_KEY="$SBCTL_DB/db.key"
        SIGN_CERT="$SBCTL_DB/db.pem"
        SIGN_MODE=db
    fi
}

write_signing_cert() {
    if [ "$SIGN_MODE" != mok ]; then
        return 0
    fi

    run mkdir -p "$KEY_DIR"
    run openssl x509 -inform der -in "$SIGN_DER" -out "$SIGN_CERT"
    run chmod 644 "$SIGN_CERT"
}

# Shim and MokManager are copied next to Aarambh and kept current, so shim
# updates (and SBAT revocations) reach Aarambh's copy too.
sync_shim() {
    local file

    if [ "${SIGN_MODE:-none}" != mok ] || [ "$SHIM_DIR" = "$ESP_DIR" ]; then
        return 0
    fi

    for file in "$SHIM_FILE" "$SHIM_MANAGER"; do
        if [ ! -f "$SHIM_DIR/$file" ]; then
            continue
        fi

        if [ ! -f "$ESP_DIR/$file" ] || {
            ! cmp -s "$SHIM_DIR/$file" "$ESP_DIR/$file" && [ "$SHIM_DIR/$file" -nt "$ESP_DIR/$file" ]
        }; then
            run cp --preserve=timestamps "$SHIM_DIR/$file" "$ESP_DIR/$file.new"
            run mv -f "$ESP_DIR/$file.new" "$ESP_DIR/$file"
        fi
    done
}

# Signs \EFI\aarambh\aarambh.efi (adding this OS's signature next to any other
# OS's) and puts it where shim looks for its second stage.
install_signed_loader() {
    local image="$ESP_DIR/aarambh.efi"

    if ! signing_enabled || [ ! -f "$image" ]; then
        return 0
    fi

    if ! signed_by_us "$image"; then
        if [ "$DRY_RUN" = 1 ]; then
            echo "  would sign: $image"
        elif ! sign_file "$image" "$image.signed"; then
            rm -f "$image.signed"
            die "signing $image failed"
        else
            mv -f "$image.signed" "$image"
        fi
    fi

    if [ "$SIGN_MODE" = mok ] && ! cmp -s "$image" "$ESP_DIR/$SHIM_SECOND"; then
        run cp "$image" "$ESP_DIR/$SHIM_SECOND.new"
        run mv -f "$ESP_DIR/$SHIM_SECOND.new" "$ESP_DIR/$SHIM_SECOND"
    fi
}

# The one-time password MokManager asks for. Eight digits, because MokManager
# reads the keyboard as a US layout and the digit keys are the same on every
# layout. Kept root-only; 'sudo aarambh-sign --password' shows it again.
ensure_password() {
    if [ -s "$PASSWORD_FILE" ]; then
        return 0
    fi

    mkdir -p "$KEY_DIR"
    chmod 700 "$KEY_DIR"
    ( umask 077; printf '%08d\n' $(( $(od -An -N4 -tu4 /dev/urandom) % 100000000 )) > "$PASSWORD_FILE" )
}

mok_password() {
    cat "$PASSWORD_FILE"
}

# The password as a SHA-512 crypt hash for mokutil, so it never appears on a
# command line.
mok_password_hash() {
    openssl passwd -6 -stdin < "$PASSWORD_FILE"
}

# With Secure Boot on, asks shim to enroll the key at the next start. Only the
# user at the keyboard can confirm it, in MokManager.
queue_enrollment() {
    if [ "${SIGN_MODE:-none}" != mok ] || ! secure_boot_on; then
        return 0
    fi

    case "$(mok_state "$SIGN_DER")" in
        enrolled|pending) return 0 ;;
    esac

    if [ "$DRY_RUN" = 1 ]; then
        echo "  would run: mokutil --import $SIGN_DER"
        return 0
    fi

    ensure_password
    if mokutil --import "$SIGN_DER" --hash-file <(mok_password_hash); then
        MOK_QUEUED=1
    fi
}

# On --purge: removes Aarambh's own key (never a DKMS, akmods or shim-signed
# key) from the trusted keys at the next start, and deletes it.
forget_own_key() {
    local der="$KEY_DIR/aarambh.der"

    if [ ! -f "$der" ]; then
        return 0
    fi

    if has_command mokutil && [ "$(mok_state "$der")" = enrolled ]; then
        if [ "$DRY_RUN" = 1 ]; then
            echo "  would run: mokutil --delete $der"
        else
            ensure_password
            if mokutil --delete "$der" --hash-file <(mok_password_hash); then
                log "at the next restart choose Delete MOK -> Continue -> Yes, password: $(mok_password)"
            fi
        fi
    fi

    run rm -rf "$KEY_DIR"
}

print_enrollment_steps() {
    if [ "$MOK_QUEUED" != 1 ]; then
        return 0
    fi

    echo
    log "Secure Boot: one step is left, only once, at the next restart."
    log "  A blue 'Perform MOK management' screen appears. Choose:"
    log "  Enroll MOK -> Continue -> Yes, type the password: $(mok_password) -> Reboot"
    log "  Forgot it? 'sudo aarambh-sign --password' shows it again. If you miss"
    log "  the screen, nothing breaks: the PC starts your OS's own boot loader"
    log "  and the screen comes back at the following restart."
}

signing_environment() {
    echo "SIGN_MODE=$SIGN_MODE"
    echo "SIGN_KEY=$SIGN_KEY"
    echo "SIGN_CERT=$SIGN_CERT"
    echo "SIGN_DER=$SIGN_DER"
    echo "SHIM_DIR=$SHIM_DIR"
    echo "SHIM_FILE=$SHIM_FILE"
    echo "SHIM_MANAGER=$SHIM_MANAGER"
    echo "SHIM_SECOND=$SHIM_SECOND"
}
