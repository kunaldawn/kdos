#!/bin/bash
# Create /run/user/<uid> for every human user (uid >= 1000).
# /run is root-owned, so a user's own profile cannot create it — but Wayland,
# rootless podman and pipewire all need it before the first login.

case "$1" in
    start)
        echo "[KDOS] Creating user runtime dirs..."
        while IFS=: read -r _name _pw uid gid _gecos _home _shell; do
            case "$uid" in
                ''|*[!0-9]*) continue ;;
            esac
            [ "$uid" -ge 1000 ] || continue
            [ "$uid" -lt 65534 ] || continue
            dir="/run/user/$uid"
            mkdir -p "$dir" || continue
            chown "$uid:$gid" "$dir"
            chmod 0700 "$dir"
            echo "[KDOS]   $dir ($uid:$gid)"
        done < /etc/passwd
        ;;
    stop|status)
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
