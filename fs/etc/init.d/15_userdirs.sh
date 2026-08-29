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

            # DELEGATE A CGROUP SUBTREE, or every resource key in a box
            # profile is decoration. Rootless podman applies --memory and
            # --cpus through cgroup2 and can only do so inside a subtree it
            # OWNS; with none delegated the flags are accepted and silently
            # do nothing, which is why kdos-oomd had to enforce `memory` by
            # victim preference. This is the arrangement systemd's user
            # slice provides and a distribution without systemd never had.
            # The controllers must be enabled at the ROOT first — a child
            # cannot enable what its parent does not offer.
            cg=/sys/fs/cgroup
            if [ -w "$cg/cgroup.subtree_control" ]; then
                echo "+cpu +memory +pids" > "$cg/cgroup.subtree_control" 2>/dev/null
                udir="$cg/user.slice/user-$uid"
                mkdir -p "$udir" 2>/dev/null
                echo "+cpu +memory +pids" > "$cg/user.slice/cgroup.subtree_control" 2>/dev/null
                chown "$uid:$gid" "$udir" "$udir/cgroup.procs" \
                      "$udir/cgroup.subtree_control" "$udir/cgroup.threads" 2>/dev/null
                # The session goes in a LEAF (kdos-getty moves the autologin
                # there), because a cgroup holding processes cannot enable
                # controllers for its children, and podman's container
                # cgroups are created as SIBLINGS of the caller's. So the
                # controllers are enabled one level up, here, where nothing
                # will ever run.
                mkdir -p "$udir/session" 2>/dev/null
                echo "+cpu +memory +pids" > "$udir/cgroup.subtree_control" 2>/dev/null
                chown "$uid:$gid" "$udir/session" "$udir/session/cgroup.procs" 2>/dev/null
                echo "[KDOS]   cgroup $udir delegated"
            fi
        done < /etc/passwd
        ;;
    stop|status)
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
