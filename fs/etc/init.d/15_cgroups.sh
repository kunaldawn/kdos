#!/bin/bash
# THE CONTROLLERS AT THE ROOT OF THE cgroup2 TREE, before anything makes a
# cgroup under it.
#
# A child can enable only what its parent offers, and under sysvinit nothing
# else ever writes the root's cgroup.subtree_control. elogind (40) makes each
# login's cgroup at the top level, /sys/fs/cgroup/<session>, and with
# DelegateSessionCgroups=yes (/etc/elogind/logind.conf.d/50-kdos.conf) enables
# for that session's subtree exactly the controllers its cgroup.controllers
# lists — which is what this root offers. Without these, every resource key in
# a box profile is decoration: rootless podman applies --memory, --cpus and
# --pids-limit through cgroup2, accepts the flags in a cgroup that has no such
# controller, and silently does nothing.
#
# One write per controller, because a kernel built without one refuses the
# whole line and would take the other two with it. The root cgroup is the one
# cgroup that may hold processes and enable controllers at once, so this works
# with every daemon of the boot already in it.
#
# The runtime directories under /run/user are not made here: turnstile makes a
# user's at the first login and removes it at the last (or keeps it for a user
# who lingers), and is the only thing that does.

CG=/sys/fs/cgroup
CONTROLLERS="cpu memory pids"

case "$1" in
    start)
        if [ ! -w "$CG/cgroup.subtree_control" ]; then
            echo "[SKIP] cgroups: $CG is not a writable cgroup2 mount"
            exit 0
        fi
        echo "[KDOS] Enabling cgroup controllers at the root..."
        _rc=0
        for _c in $CONTROLLERS; do
            if ! grep -qw "$_c" "$CG/cgroup.controllers" 2>/dev/null; then
                echo "[WARN] cgroups: this kernel has no $_c controller"
                _rc=1
                continue
            fi
            echo "+$_c" > "$CG/cgroup.subtree_control" 2>/dev/null || {
                echo "[WARN] cgroups: could not enable $_c at the root"
                _rc=1
            }
        done
        echo "[KDOS]   $CG: $(cat "$CG/cgroup.subtree_control" 2>/dev/null)"
        exit "$_rc"
        ;;
    stop)
        ;;
    status)
        _on=$(cat "$CG/cgroup.subtree_control" 2>/dev/null)
        for _c in $CONTROLLERS; do
            case " $_on " in
                *" $_c "*) ;;
                *) echo "[DOWN] cgroups: $_c not enabled at the root ($_on)"
                   exit 1 ;;
            esac
        done
        echo "[ OK ] cgroups: $_on"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
