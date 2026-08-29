#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------
#
# Sweep the application catalogue on a booted KDOS: install each app pack off
# the medium, launch it through its own shim, wait for the compositor to report
# a window, photograph it, and tear the box down again.
#
# RUNS INSIDE THE GUEST, as root, with a desktop session already up.
#
#   appsweep.sh <outdev> <runtime|all> [max]
#
# WHAT IT ASSERTS IS THE FIVE THINGS THAT BREAK IN THIS LANE, and nothing
# past them: the pack mounts, the box starts, a launcher exists, a WINDOW MAPS
# with the compositor naming both its app_id and its box, and the shot is not
# blank. Whether the application is CORRECT is not a thing a screenshot knows,
# and a sweep that claimed it would be 183 green ticks worth less than they
# look.
#
# THREE STATES, NEVER TWO. `skip` with a reason is a result — an app that wants
# a GPU, a camera or a network on a rig that has none is not a failure of the
# pack lane, and recording it as one would make the number meaningless in the
# direction that flatters us.
#
# RESUMABLE. Every app appends its row to the TSV before the next one starts,
# so a batch that dies halfway has recorded everything up to that point and a
# re-run skips what is already there. /var/tmp is on the installed root, not on
# tmpfs, or that promise would end at the next boot.

set -u

DEV="${1:?usage: appsweep.sh <outdev> <runtime|all> [max]}"
WANT="${2:-all}"
MAX="${3:-0}"

OUT=/var/tmp/sweep
TSV="$OUT/results.tsv"
MEDIUM=/mnt/iso/packs
U=kdos
RTDIR=/run/user/1000
# A launch that never maps a window must cost this, not the run.
LAUNCH_TIMEOUT=${LAUNCH_TIMEOUT:-45}
SETTLE=6

asuser() { su - "$U" -c "XDG_RUNTIME_DIR=$RTDIR WAYLAND_DISPLAY=wayland-0 $1" 2>&1; }

note() { printf '%s\n' "$*" >&2; }

purge() {
    # KILL THE APPLICATION, NOT JUST THE CONTAINER. A box shares the host's
    # PID namespace (`processes = shared` is the default), so `podman rm -f`
    # ends pid 1 of the box and leaves the application it exec'd running as
    # an orphan — measured on the contact sheet, where LibreOffice's start
    # centre sat under five later applications' frames, and as `Killed` on
    # a Java app once forty of them had piled up in a 4 GB machine. Every
    # process a box starts carries KDOS_BOX=<name> in its environment, which
    # is the one marker that names the box and nothing else.
    for p in /proc/[0-9]*; do
        # the redirect is the shell's, so a process that exited between the
        # glob and the open prints "No such process" from the SHELL and a
        # `2>/dev/null` on tr does not catch it — the block's does
        { tr '\0' '\n' < "$p/environ" | grep -qx "KDOS_BOX=$1"; } 2>/dev/null \
            && kill -KILL "${p#/proc/}" 2>/dev/null
    done
    asuser "kdos-box remove $1 --force" >/dev/null 2>&1
    asuser "podman rm -f $1" >/dev/null 2>&1
    asuser "kdos app remove $1" >/dev/null 2>&1
    rm -rf "/home/$U/.local/share/kdos/boxes/$1" \
           "/home/$U/.config/kdos/boxes/$1.conf" "/tmp/$1.log"
}

# ── the medium, which nothing mounts on an installed system ────────────────
#
# `/mnt/iso` is the LIVE session's arrangement: the initramfs mounts the medium
# and MS_MOVEs it across switch_root. A machine booted from disk with the same
# stick in the drive has no such mount and no reason to make one — so the packs
# are sitting on a device nobody opened, and a sweep that assumed otherwise
# reads whatever stale directory happens to be at that path. Measured: a
# leftover one-entry PACKAGES made a 72-app batch look like a 1-app batch, and
# the sweep reported it as a complete success.
if ! grep -q ' /mnt/iso ' /proc/mounts 2>/dev/null; then
    mkdir -p /mnt/iso
    for d in /dev/sr0 /dev/sr1 /dev/cdrom; do
        [ -b "$d" ] || continue
        mount -o ro "$d" /mnt/iso 2>/dev/null && break
    done
fi
have=$(grep -c '^P:' "$MEDIUM/PACKAGES" 2>/dev/null || echo 0)
note "== medium: $have pack(s) at $MEDIUM"
[ "$have" -ge 10 ] || { note "== the medium is not mounted — refusing to sweep"; exit 2; }

# Anything a previous batch left is removed before this one starts: a machine
# carrying dozens of exited containers is a machine whose next boot is slow,
# and the sweep must not be the reason its own results drift.
for stale in $(asuser "podman ps -aq --format '{{.Names}}'" 2>/dev/null | grep '^app\.' ); do
    purge "$stale"
done

mkdir -p "$OUT"
chown -R "$U:$U" "$OUT" 2>/dev/null || true
[ -f "$TSV" ] || printf 'id\tstate\treason\tapp_id\tbox\tms\tbytes\n' > "$TSV"

# ── which packs are in this batch ──────────────────────────────────────────
#
# Read from the medium's own index rather than from a list kept here: a second
# copy of the catalogue is a second thing to drift, and the index is what the
# installer and `kdos app` already read.
batch() {
    # `id:a b c` sweeps exactly those, which is how a failure is re-verified on
    # its own after a fix rather than by re-running its whole runtime.
    case "$WANT" in
        id:*) printf '%s\n' "${WANT#id:}" | tr ' ' '\n' | grep -v '^$'; return ;;
    esac
    awk -v want="$WANT" '
        /^P:/ { id = substr($0, 3); k = ""; d = "" }
        /^K:/ { k = substr($0, 3) }
        /^D:/ { d = substr($0, 3) }
        /^$/  { if (k == "app" && (want == "all" || d == want)) print id; id = "" }
        END   { if (id != "" && k == "app" && (want == "all" || d == want)) print id }
    ' "$MEDIUM/PACKAGES"
}

done_already() { cut -f1 "$TSV" | grep -qx "$1"; }

record() {  # id state reason app_id box ms bytes
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" "$5" "$6" "$7" >> "$TSV"
    note "  $1: $2 ${3:+($3)}"
}

# ── the sweep ──────────────────────────────────────────────────────────────
ids=$(batch)
[ -n "$ids" ] || { note "no app packs match '$WANT'"; exit 1; }
total=$(printf '%s\n' "$ids" | wc -l)
note "== sweeping $total app pack(s) for '$WANT'"

n=0
for id in $ids; do
    done_already "$id" && continue
    n=$((n + 1))
    [ "$MAX" != 0 ] && [ "$n" -gt "$MAX" ] && break
    note "-- [$n/$total] $id"

    # 1. the pack mounts. The WHOLE output is tested, not its last line:
    #    `kdos app install` now regenerates the user's launchers as its final
    #    act and the last line is genlaunchers' count, which says nothing
    #    about whether the mount happened.
    out=$(asuser "kdos app install $id")
    case "$out" in
        *mounted*|*installed*|*already*) ;;
        *) record "$id" fail "install: $(printf '%s' "$out" | tail -1)" - - 0 0
           purge "$id"; continue ;;
    esac

    # 2. a launcher exists. The table's THIRD field is the pack, which is why
    #    it has one — matching on the shim name would guess.
    # `kdos app install` regenerated the USER tree (G1); the shim is there.
    # A PACK MAY SHIP MORE THAN ONE APPLICATION, so take the launcher whose
    # NAME matches the pack rather than whichever sorted first: `app.thunderbird`
    # is `thunderbird claws-mail` in packs.conf, and the first row tested
    # claws-mail while the result was recorded against thunderbird. LibreOffice
    # ships eight. Falls back to the first row when nothing matches, which is
    # right for a pack whose one application is named differently from it.
    want=${id#app.}
    shim=$(awk -F'\t' -v p="$id" -v w="$want" '
            $3 == p { if ($1 == w) { print $1; found = 1; exit }
                      if (first == "") first = $1 }
            END { if (!found && first != "") print first }' \
           "/home/$U/.local/share/kdos/alien-apps" 2>/dev/null)
    if [ -z "$shim" ] || [ ! -e "/home/$U/.local/bin/$shim" ]; then
        # A COMMAND-ONLY PACK HAS NO WINDOW TO WAIT FOR. gmic and ngspice
        # ship a `command =` and no desktop entry — the wine shape — and a
        # sweep that scored "no launcher" against them was scoring the
        # absence of something they were never meant to have.
        if asuser "kdos app show $id" | grep -qE '^command'; then
            record "$id" skip "command-only pack (no desktop entry)" - - 0 0
        else
            record "$id" fail "no launcher" - - 0 0
        fi
        asuser "kdos app remove $id" >/dev/null 2>&1
        continue
    fi

    # 3/4. launch, and wait for the compositor to name the window.
    # A `Terminal=true` entry is launched the way the Start menu launches
    # it — in foot — and the window to wait for is foot's: R with a pipe for
    # stdin exits on "you must specify --save", which is R being right.
    t0=$(date +%s%3N 2>/dev/null || echo 0)
    term=""
    if grep -ls "^Exec=kdos-appbox run $shim\( \|$\)" \
         /home/$U/.local/share/applications/*.desktop 2>/dev/null \
       | xargs -r grep -lq "^Terminal=true" 2>/dev/null; then
        term=foot
        asuser "nohup foot -e /home/$U/.local/bin/$shim >/tmp/$id.log 2>&1 &" >/dev/null 2>&1
    else
        asuser "nohup $shim >/tmp/$id.log 2>&1 &" >/dev/null 2>&1
    fi
    appid=""; waited=0
    while [ "$waited" -lt "$LAUNCH_TIMEOUT" ]; do
        sleep 3; waited=$((waited + 3))
        # the fifth column is the box; an exact match, because `kdos hey
        # list` used to truncate it to twelve characters and a substring
        # match on the whole line scored every long-named box as no window
        # AN X11 WINDOW HAS NO BOX. Its Wayland client is Xwayland, on the
        # host, with no security context, so the box column reads `-` and
        # app_id is the WM_CLASS — hexchat, gerbv, bcnc. Measured: eleven
        # X11-toolkit applications were running with a window on screen and
        # scored "no window" because nothing on the line said which box.
        # They match on app_id against the shim's name instead, and the
        # row records that the attribution was by name.
        line=$(asuser "kdos hey list" | awk -v b="$id" -v sh="$shim" -v t="$term" '
            $5 == b { print; exit }
            $5 == "-" && tolower($4) == tolower(sh) { print; exit }
            t != "" && $4 == t { print; exit }')
        [ -n "$line" ] && { appid=$(printf '%s' "$line" | awk '{print $4}'); break; }
    done
    t1=$(date +%s%3N 2>/dev/null || echo 0)
    ms=$((t1 - t0))
    # The launch's whole stdout+stderr travels back with the shots. The row
    # carries one line of it, and one line is what a failure is diagnosed
    # FROM when the machine it happened on is a VM that has already been
    # torn down.
    mkdir -p "$OUT/logs"; cp "/tmp/$id.log" "$OUT/logs/$id.log" 2>/dev/null

    if [ -z "$appid" ]; then
        log=$(cat "/tmp/$id.log" 2>/dev/null)
        why=$(printf '%s' "$log" | tail -1 | cut -c1-70)
        # A CAPABILITY THIS RIG DOES NOT HAVE IS `skip`, NOT `fail`, and the
        # patterns are deliberately narrow: each names a thing the MACHINE
        # lacks, not a thing the pack lane got wrong. Recording these as
        # failures would move the number in the direction that flatters us
        # while making it mean nothing — and recording them as passes would be
        # worse. `kdos doctor`'s three-state rule, on a different subject.
        case "$log" in
            *GLX*|*"GetGLXVersion"*|*"Could not find a decent"*)
                record "$id" skip "no GLX: Xwayland is built -Dglx=false" - - "$ms" 0 ;;
            *"libGL error"*|*"failed to load driver"*|*"Failed to initialize EGL"*|\
            *"OpenGL 3"*|*"GL context"*|*"does not seem to support OpenGL"*|*"requires OpenGL"*|\
            *"Could not initialize display"*)
                record "$id" skip "no GPU on this rig (pixman)" - - "$ms" 0 ;;
            *"Could not resolve host"*|*"Network is unreachable"*|*"Temporary failure in name"*)
                record "$id" skip "offline rig" - - "$ms" 0 ;;
            *)
                case "$why" in
                *"No such file or directory"*)
                    record "$id" fail "missing binary: ${why}" - - "$ms" 0 ;;
                *)
                    record "$id" fail "no window: ${why:-timeout}" - - "$ms" 0 ;;
                esac ;;
        esac
    else
        # 5. it drew something — the blank check is the host's, on the pixels
        sleep "$SETTLE"
        shot="$OUT/$id.png"
        asuser "grim $shot" >/dev/null 2>&1
        bytes=$(stat -c %s "$shot" 2>/dev/null || echo 0)
        if [ "$bytes" = 0 ]; then
            record "$id" fail "no shot" "$appid" "$id" "$ms" 0
        else
            record "$id" ok "" "$appid" "$id" "$ms" "$bytes"
        fi
    fi

    # 6. TEAR DOWN EVERY TIME, AND PURGE — `kdos-box remove` DELIBERATELY
    #    KEEPS the profile and the writable upper, which is right for a person
    #    who may want them back and wrong for a sweep that will do this 183
    #    times. Measured after one 8-app batch: every container was still
    #    there as `exited`, every profile still listed, and the NEXT boot's
    #    desktop never came up inside the rig's 240 s deadline. So the sweep
    #    removes what it made, all of it, rather than leaving the machine to
    #    accumulate a container per application.
    purge "$id"
done

chown -R "$U:$U" "$OUT" 2>/dev/null || true
note "== $(grep -c . "$TSV") row(s); writing the tar to $DEV"
tar cf "$DEV" -C /var/tmp sweep 2>/dev/null && note "== written" || note "== TAR FAILED"
sync
