#!/usr/bin/env bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/siteshots.sh — the screenshot gallery of kdos.dev
#
#   testing/siteshots.sh [RUN...]     photograph these runs, then convert
#   testing/siteshots.sh --convert    convert what build/siteshots holds
#
# Boots build/iso-build/kdos.iso on the rig, takes every picture of the site's
# gallery, and writes site/assets/shots/: each shot as a full-size WebP and a
# 640-pixel-wide thumbnail, the kdos-bb demo as an animated WebP, and
# shots.tsv, a title, a caption and an alt text per shot; site/home.md's
# gallery sets the order the page shows them in. The raw frames stay in
# build/siteshots/<run>/ for review.
#
# A RUN IS ONE BOOT. The runs, in the order they are taken:
#
#   boot       the Limine menu and the boot splash, from power-on; QEMU is
#              driven directly, because vnc-shot.py starts typing only once
#              the kernel has the serial console, long after the menu
#   desktop    the live desktop, the Start menu, Konsole, the applications,
#              the Overview, KRunner, System Settings and Discover
#   accents    the desktop in every accent but the default, Phosphor mode, the
#              dock and tiling desks, and the lock screen
#   installer  the graphical installer's pages, with a blank disk attached
#   console    the text console's login banner and the kdos-bb demo
#   installed  the SDDM greeter, on an installed disk
#
# Needs the rig image (testing/rig-image.sh), /dev/kvm, /dev/dri for the runs
# that use --gl, and python3 with Pillow built with WebP on the host. The
# installed run boots a throwaway qcow2 overlay of an installed disk:
# KDOS_SITESHOTS_DISK (default build/kdos-target.qcow2), made by
# testing/install-to-disk.sh from the same ISO, since the greeter names the
# release it runs.
# KDOS_RIG_IMAGE names another rig image; KDOS_SITESHOTS_DOCKER_ARGS adds
# arguments to every `docker run`, such as `--gpus all` for NVIDIA's own EGL.
#
# Every picture is the shipped system as it starts: the only things the runs
# put on the machine are files a user would have (the accent wallpapers copied
# into ~/Pictures, the kdos-doc(1) manual printed to a PDF in ~/Documents) and the
# window positions, which a KWin script sets so that every release's pictures
# frame the same way. Look at every picture before publishing it: a run that
# passes can still have photographed a half-drawn window.
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=${KDOS_RIG_IMAGE:-kdos-qemu-py:latest}
DISK=${KDOS_SITESHOTS_DISK:-build/kdos-target.qcow2}
RAW=build/siteshots
OUT=site/assets/shots
ALL_RUNS="boot desktop accents installer console installed"

usage() {
    sed -n '9,13p' "$0" | sed 's/^# \{0,1\}//'
    echo "runs: $ALL_RUNS"
}

convert_only=0
runs=()
for a in "$@"; do
    case $a in
        --convert) convert_only=1 ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *)
            case " $ALL_RUNS " in
                *" $a "*) runs+=("$a") ;;
                *) echo "siteshots: no run named $a" >&2; usage >&2; exit 2 ;;
            esac
            ;;
    esac
done
[ "$convert_only" = 1 ] || [ "${#runs[@]}" -gt 0 ] || read -r -a runs <<<"$ALL_RUNS"

python3 -c 'from PIL import features; assert features.check("webp")' 2>/dev/null || {
    echo "siteshots: python3 with Pillow built with WebP is needed to convert" >&2
    exit 1
}

# The accents, from libkcolor's table: every name but the default's, in the
# table's order. A new accent is photographed without this file changing.
DEFAULT_ACCENT=$(sed -n 's/^#define KCOL_DEFAULT_ID \([a-z]*\)$/\1/p' src/libs/libkcolor/kcolor.h)
mapfile -t ACCENTS < <(sed -n 's/^[[:space:]]*X(\([a-z][a-z]*\), ".*/\1/p' src/libs/libkcolor/kcolor.h |
                       grep -vx "$DEFAULT_ACCENT")

# ── the rig ─────────────────────────────────────────────────────────────
#
# AS THE INVOKING USER, so every frame and overlay under build/ is the
# user's to delete; the container gets the device nodes' groups instead of
# root. Each container is named after this script's process, and the trap
# removes whatever is still running when the script stops.

containers=()
cleanup() {
    for c in "${containers[@]}"; do
        docker rm -f "$c" >/dev/null 2>&1 || true
    done
}
trap cleanup EXIT

docker_args() {   # docker_args gl|plain
    local args=(--rm --user "$(id -u):$(id -g)" -e HOME=/tmp
                --device /dev/kvm --group-add "$(stat -c %g /dev/kvm)"
                -v "$PWD:/kdos" -w /kdos)
    if [ "$1" = gl ]; then
        args+=(--device /dev/dri --group-add "$(stat -c %g "$RENDER")")
    fi
    # shellcheck disable=SC2206 # extra docker arguments, split as words on purpose
    args+=(${KDOS_SITESHOTS_DOCKER_ARGS:-})
    printf '%s\n' "${args[@]}"
}

# rig RUN gl|plain [MOUNT...] -- VNC-SHOT ARGS...
rig() {
    local run=$1 mode=$2
    shift 2
    local extra=()
    while [ "$1" != -- ]; do extra+=(-v "$1"); shift; done
    shift
    local name="siteshots-$run-$$"
    containers+=("$name")
    local args
    mapfile -t args < <(docker_args "$mode")
    local gl=()
    [ "$mode" = gl ] && gl=(--gl)
    echo "==> $run"
    docker run --name "$name" "${args[@]}" "${extra[@]}" "$IMAGE" \
        python3 testing/vnc-shot.py --size 1920x1080 "${gl[@]}" \
        --serial-log "$RAW/$run/serial.log" "$@"
}

# ── what runs in the guest ──────────────────────────────────────────────
#
# Sent by --root-script, as root, before any picture. site-place moves the
# newest normal window whose class or desktop file names MATCH to X,Y with
# size WxH and activates it, through a KWin script loaded over the session
# bus, so every window of every release is framed alike and a typed command
# reaches the window just placed. site-konsole-bare puts the newest Konsole
# full screen and hides its toolbars, through the window's own calls on the
# session bus. Both run as the session's user, by --cmd.

write_guest_setup() {
    cat >"$RAW/guest-setup.sh" <<'GUEST'
set -u
user=${1:-kdos}
home=$(awk -F: -v u="$user" '$1 == u { print $6 }' /etc/passwd)
cat > /run/kdos-rig/site-place <<'PLACE'
#!/bin/sh
m=$1 x=$2 y=$3 w=$4 h=$5
js=$XDG_RUNTIME_DIR/site-place.js
cat > "$js" <<JS
var ws = workspace.windowList();
for (var i = ws.length - 1; i >= 0; i--) {
    var c = ws[i];
    if (!c.normalWindow) continue;
    var id = (c.resourceClass + " " + c.resourceName + " " + c.desktopFileName).toLowerCase();
    if (id.indexOf("$m") < 0) continue;
    c.setMaximize(false, false);
    c.frameGeometry = {x: $x, y: $y, width: $w, height: $h};
    workspace.activeWindow = c;
    break;
}
JS
id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$js" site-place)
qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run
sleep 1
qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript site-place >/dev/null
PLACE
chmod 0755 /run/kdos-rig/site-place
cat > /run/kdos-rig/site-konsole-bare <<'BARE'
#!/bin/sh
svc=org.kde.konsole-$(pgrep -nx konsole)
win=/konsole/MainWindow_1
qdbus6 "$svc" "$win" org.kde.konsole.Konsole.MainWindow.viewFullScreen true
for t in $(qdbus6 "$svc" "$win" org.kde.konsole.KXmlGuiWindow.toolBars); do
    qdbus6 "$svc" "$win" org.kde.konsole.KXmlGuiWindow.setToolBarVisible "$t" false
done
BARE
chmod 0755 /run/kdos-rig/site-konsole-bare
mkdir -p "$home/Documents" "$home/Pictures"
mandoc -T ps /usr/share/man/man1/kdos-doc.1 | ps2pdf - "$home/Documents/kdos-doc.pdf"
for d in /usr/share/wallpapers/KDOS-*/; do
    a=$(basename "$d")
    cp "$d/contents/images_dark/1920x1080.png" "$home/Pictures/$a.png"
done
chown -R "$user:" "$home/Documents" "$home/Pictures"
echo "site setup: $(ls "$home/Pictures" | wc -l) wallpapers, $(wc -c < "$home/Documents/kdos-doc.pdf") byte manual"
GUEST
}

PLACE=/run/kdos-rig/site-place
SETUP=(--root-script "$RAW/guest-setup.sh kdos")

# ── the runs ────────────────────────────────────────────────────────────

# LIMINE AND THE SPLASH ARE PHOTOGRAPHED BY TIME FROM POWER-ON, through the
# monitor's screendump, and nothing is ever written to the serial line:
# Limine reads it as input and a byte there stops the countdown. The menu is
# up from about 2 s to the end of its 10-second countdown, so 5 s is inside it
# with room either side. The firmware and Limine draw at the firmware's
# 1280x800; the kernel's mode switch to 1920x1080 is the splash's first frame,
# and 7 s after it the splash is about a third of the way through rcS.
run_boot() {
    local name="siteshots-boot-$$" args
    containers+=("$name")
    mapfile -t args < <(docker_args plain)
    echo "==> boot"
    docker run --name "$name" -i "${args[@]}" "$IMAGE" python3 - "$RAW/boot" <<'PY'
import importlib.util, os, subprocess, sys, tempfile, time
spec = importlib.util.spec_from_file_location("vs", "testing/vnc-shot.py")
vs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vs)
out = sys.argv[1]
MENU_AT, SPLASH_AFTER, LIMIT = 5.0, 7.0, 120.0
ovmf = next(f for f in ("/usr/share/ovmf/OVMF.fd", "/usr/share/OVMF/OVMF.fd",
                        "/usr/share/qemu/OVMF.fd") if os.path.isfile(f))
run = tempfile.mkdtemp()
ser_sock, mon_sock = run + "/serial.sock", run + "/monitor.sock"
vm = subprocess.Popen([
    "qemu-system-x86_64", "-enable-kvm", "-cpu", "host",
    "-smp", str(min(8, os.cpu_count() or 4)), "-m", "4G", "-bios", ovmf,
    "-vga", "none", "-device", "virtio-vga,xres=1920,yres=1080",
    "-display", "none", "-usb", "-device", "usb-tablet", "-cdrom", vs.ISO,
    "-chardev", "socket,id=ser0,path=%s,server=on,wait=off" % ser_sock,
    "-serial", "chardev:ser0",
    "-chardev", "socket,id=mon0,path=%s,server=on,wait=off" % mon_sock,
    "-monitor", "chardev:mon0",
    "-netdev", "user,id=net0", "-device", "virtio-net-pci,netdev=net0",
], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
t0 = time.time()
try:
    if not vs.wait_for(mon_sock) or not vs.wait_for(ser_sock):
        sys.exit("qemu did not create its sockets")
    mon, ser = vs.Monitor(mon_sock), vs.Serial(ser_sock)

    def dump(path):
        mon.cmd("screendump " + path, deadline=10.0)
        with open(path, "rb") as f:
            return f.readline() and tuple(int(v) for v in f.readline().split())

    while time.time() - t0 < MENU_AT:
        ser.pump()
        time.sleep(0.1)
    print("limine %dx%d at %.1fs" % (dump(out + "/limine.ppm") + (time.time() - t0,)))
    probe, mode = out + "/probe.ppm", None
    while time.time() - t0 < LIMIT:
        ser.pump()
        if mode is None and dump(probe) == (1920, 1080):
            mode = time.time()
        if mode is not None and time.time() - mode >= SPLASH_AFTER:
            dump(out + "/splash.ppm")
            print("splash %.1fs after the mode switch" % (time.time() - mode))
            break
        time.sleep(0.25)
    else:
        sys.exit("no mode switch within %ds" % LIMIT)
    os.remove(probe)
finally:
    vm.terminate()
    vm.wait(10)
PY
}

# Konsole is opened with Meta+Return, as a person opens it, and not by --cmd:
# a program --cmd starts belongs to the login session `su` opens, and `kdos
# version` and `kdos status` in a terminal started that way report that tty
# session instead of the desktop's.
run_desktop() {
    local d=$RAW/desktop
    rig desktop gl -- "${SETUP[@]}" \
        --sleep 8 --shot "$d/welcome.ppm" \
        --dismiss-picker \
        --shot "$d/desktop.ppm" \
        --keys meta_l --sleep 2 --shot "$d/start-menu.ppm" --keys esc --sleep 1 \
        --keys meta_l-ret --sleep 2 --cmd "$PLACE konsole 640 60 1040 880" \
        --type 'clear; fastfetch' --sleep 3 --shot "$d/terminal.ppm" \
        --cmd "$PLACE konsole 460 150 1000 720" \
        --type 'clear; kdos version; kdos theme list; kdos status' --sleep 3 \
        --shot "$d/kdos-command.ppm" \
        --root-cmd 'pkill -x konsole' \
        --cmd 'dolphin ~/Pictures' --sleep 3 --cmd "$PLACE dolphin 210 60 1500 920" \
        --keys ctrl-equal --keys ctrl-equal --sleep 2 --shot "$d/dolphin.ppm" \
        --cmd 'kate --line 160 /ports/src/art/kdos-bb/src/bb.c' --sleep 4 \
        --cmd "$PLACE kate 210 60 1500 920" --sleep 2 --shot "$d/kate.ppm" \
        --keys meta_l-ret --sleep 2 --cmd "$PLACE konsole 300 200 1000 640" \
        --type 'fastfetch' --sleep 2 \
        --chord meta+w --sleep 3 --shot "$d/overview.ppm" --keys esc --sleep 2 \
        --root-cmd 'pkill -x konsole; pkill -x dolphin; pkill -x kate' --sleep 2 \
        --chord alt+space --sleep 2 --text 'accent' --sleep 3 --shot "$d/krunner.ppm" \
        --keys esc --sleep 1 \
        --cmd 'okular ~/Documents/kdos-doc.pdf' --sleep 4 --cmd "$PLACE okular 210 60 1500 920" \
        --sleep 2 --shot "$d/okular.ppm" --root-cmd 'pkill -x okular' \
        --cmd 'systemsettings kcm_kdos_look' --sleep 5 \
        --cmd "$PLACE systemsettings 210 60 1500 920" --sleep 2 \
        --shot "$d/settings-look.ppm" --root-cmd 'pkill -x systemsettings' \
        --cmd 'plasma-discover' --sleep 5 --cmd "$PLACE discover 210 60 1500 920" \
        --sleep 50 --shot "$d/discover.ppm" --root-cmd 'pkill -x plasma-discover' \
        --cmd 'plasma-discover --application appstream://scribus.desktop' --sleep 5 \
        --cmd "$PLACE discover 210 60 1500 920" --sleep 40 --shot "$d/discover-app.ppm"
}

run_accents() {
    local d=$RAW/accents steps=() a
    for a in "${ACCENTS[@]}"; do
        steps+=(--cmd "$PLACE konsole 1250 60 630 400" --type clear
                --type "kdos theme $a" --sleep 10 --type 'kdos theme list'
                --keys meta_l --sleep 2 --shot "$d/accent-$a.ppm" --keys esc --sleep 1)
    done
    # Phosphor mode draws the pointer into the picture, so it is parked on the
    # screen's right edge, which has no edge action, before that shot.
    rig accents gl -- "${SETUP[@]}" --dismiss-picker \
        --keys meta_l-ret --sleep 2 \
        "${steps[@]}" \
        --cmd "$PLACE konsole 1250 60 630 400" \
        --type "clear; kdos theme $DEFAULT_ACCENT" --sleep 12 \
        --cmd 'dolphin ~/Pictures' --sleep 3 --cmd "$PLACE dolphin 60 60 1100 700" \
        --cmd "$PLACE konsole 820 110 1040 880" \
        --type 'kdos phosphor on --preset retro' --sleep 10 \
        --type 'clear; fastfetch' --sleep 3 --mouse 1919,540 \
        --shot "$d/phosphor-mode.ppm" \
        --type 'kdos phosphor off' --sleep 8 \
        --root-cmd 'pkill -x dolphin; pkill -x konsole' \
        --cmd 'kdos desk dock' --sleep 12 --shot "$d/desk-dock.ppm" \
        --cmd 'kdos desk tiling' --sleep 10 \
        --keys meta_l-ret --sleep 3 --type htop --sleep 2 \
        --cmd 'dolphin ~/Pictures' --sleep 4 \
        --cmd 'kate --line 160 /ports/src/art/kdos-bb/src/bb.c' --sleep 6 \
        --shot "$d/desk-tiling.ppm" \
        --root-cmd 'pkill -x dolphin; pkill -x kate; pkill -x konsole' \
        --cmd 'kdos desk kdos' --sleep 10 \
        --chord meta+l --sleep 4 --keys shift --sleep 2 --shot "$d/lock-screen.ppm"
}

# The installer only reads until the Summary's Install button, which no step
# presses: the blank disk is there so the Disk page has one that can take the
# install, and nothing is ever written to it. Alt+Right is the window's own
# Next. The one click is the Account page's first field, with the window at
# its own size, centred on a 1920x1080 screen.
run_installer() {
    local d=$RAW/installer args pass='correct horse battery'
    mapfile -t args < <(docker_args plain)
    docker run "${args[@]}" "$IMAGE" qemu-img create -q -f qcow2 "$d/blank.qcow2" 40G
    rig installer gl -- --disk "$d/blank.qcow2" --allow-disk-growth --dismiss-picker \
        --cmd kdos-installer-gui --sleep 40 --shot "$d/installer-welcome.ppm" \
        --keys alt-right --keys alt-right --sleep 8 --shot "$d/installer-disk.ppm" \
        --keys alt-right --keys alt-right \
        --click 1119,298 --text 'Ada Lovelace' \
        --keys tab --chord ctrl+a --text ada \
        --keys tab --text "$pass" --keys tab --text "$pass" \
        --keys alt-right --mouse 1800,960 --sleep 2 --shot "$d/installer-look.ppm"
    rm -f "$d/blank.qcow2"
}

# kdos-bb runs in Konsole full screen with its toolbars hidden, started by
# the Return that ends the typed command. Its schedule is fixed by the demo's
# own clock (see kdos-bb.md), so the frames are taken at fixed times from
# that Return: the opening titles from 26 seconds, which the animation is
# made of, and the first portrait from about 92. Without --gl, so that
# --burst reads frames with screendump.
run_console() {
    local d=$RAW/console
    rig console plain -- "${SETUP[@]}" --dismiss-picker \
        --chord ctrl+alt+f2 --sleep 3 --type kdos --sleep 2 --type kdos --sleep 8 \
        --shot "$d/console.ppm" \
        --chord ctrl+alt+f1 --sleep 3 \
        --keys meta_l-ret --sleep 2 --cmd /run/kdos-rig/site-konsole-bare --sleep 2 \
        --type 'clear; kdos-bb -nosound' \
        --sleep 24 --burst 30,0.5,"$d/bb" \
        --sleep 44 --burst 6,2,"$d/face"
}

run_installed() {
    local d=$RAW/installed
    local disk
    disk=$(readlink -f "$DISK")
    [ -f "$disk" ] || { echo "siteshots: no installed disk at $DISK (KDOS_SITESHOTS_DISK)" >&2; exit 1; }
    local args
    mapfile -t args < <(docker_args plain)
    rm -f "$d/disk.qcow2"
    docker run "${args[@]}" -v "$(dirname "$disk"):$(dirname "$disk"):ro" "$IMAGE" \
        qemu-img create -q -f qcow2 -b "$disk" -F qcow2 "$d/disk.qcow2"
    rig installed gl "$(dirname "$disk"):$(dirname "$disk"):ro" -- \
        --disk "$d/disk.qcow2" --no-cdrom --until greeter \
        --sleep 4 --shot "$d/greeter.ppm"
    rm -f "$d/disk.qcow2"
}

# ── converting ──────────────────────────────────────────────────────────

convert() {
    python3 - "$RAW" "$OUT" <<'PY'
import os, re, sys
from PIL import Image

raw, out = sys.argv[1], sys.argv[2]
FULL_Q, THUMB_W = 80, 640

head = open("src/libs/libkcolor/kcolor.h").read()
default = re.search(r"^#define KCOL_DEFAULT_ID (\w+)$", head, re.M).group(1)
accents = [m.group(1) for m in re.finditer(r'^\s*X\((\w+), "', head, re.M)
           if m.group(1) != default]
# Each accent's character, in the words of theming.md's table; an accent not
# named here is still photographed and captioned by its name alone.
LOOK = {
    "phosphor": ("green on near-black", ", like an early monochrome monitor"),
    "amber": ("amber on warm black", ", the other classic monochrome monitor"),
    "ice": ("cyan and pale blue on blue-black", ""),
    "norton": ("yellow and cyan on deep blue", ", after the Norton Commander file manager"),
    "borland": ("cyan and yellow on dark teal", ", after the Borland text-mode IDEs"),
    "perfect": ("white on blue", ", after the WordPerfect editing screen"),
    "paper": ("blue ink on off-white paper", ", the one light accent"),
}


def accent_shot(a):
    t = a.capitalize()
    if a in LOOK:
        colours, after = LOOK[a]
        caption = "%s puts the whole desktop in %s%s." % (t, colours, after)
        alt = "The desktop in the %s accent, %s, " % (t, colours)
    else:
        caption = "The %s accent recolours Plasma, the terminals and the wallpaper at once." % t
        alt = "The desktop in the %s accent, " % t
    alt += ("with the Start menu open on the left and a Konsole window top right "
            "showing kdos theme %s and the list of accents." % a)
    return ("accent-" + a, "accents/accent-%s.ppm" % a, "The %s accent" % t, caption, alt)

# The pictures: name, raw file, title, caption, alt. ANIM marks
# where the animation goes.
ANIM = "kdos-bb"
SHOTS = [
    ("desktop", "desktop/desktop.ppm", "The live desktop",
     "The live desktop: KDE Plasma in Bone, the default accent, with the KDOS mascot as wallpaper.",
     "The KDOS desktop: a black wallpaper with a muscular penguin and the KDOS logo, an Install KDOS icon top left, and a panel along the bottom with Start, four workspaces, pinned applications, system meters and a clock."),
    ("terminal", "desktop/terminal.ppm", "Konsole and fastfetch",
     "Konsole running fastfetch, which names the system, the desktop, the accent and the hardware.",
     "A Konsole window in the middle of the desktop showing fastfetch: the KDOS logo and a pixel-art penguin beside lines for the OS, kernel, desktop, shell, accent, packages, CPU, memory and disk."),
    ("start-menu", "desktop/start-menu.ppm", "The Start menu",
     "The Start menu opens on favourites, with every application sorted into categories down the side.",
     "The Start menu open above the panel: favourites Firefox, Konsole, Dolphin, System Settings and Discover, categories from Development to Utilities, and the power buttons along its foot."),
    ("overview", "desktop/overview.ppm", "The Overview",
     "The Overview spreads out every window and the workspaces, and searches as you type.",
     "The Overview: Konsole, Kate and Dolphin shown side by side over the wallpaper, four workspace thumbnails along the top and a search field."),
    ("krunner", "desktop/krunner.ppm", "KRunner, the search box",
     "KRunner launches applications, runs commands and changes settings from one search box: typing accent offers the accents and the KDOS Look page.",
     "The KRunner search box at the top of the screen with the word accent typed, listing Switch the accent to the next one, to Phosphor, Amber, Ice, Bone, Norton and Borland, and the KDOS Look settings page."),
    ("dolphin", "desktop/dolphin.ppm", "Dolphin",
     "Dolphin, the file manager, with previews of the wallpaper each accent brings.",
     "Dolphin showing a Pictures folder of eight wallpaper previews, one per accent, with Places down the left side."),
    ("kate", "desktop/kate.ppm", "Kate",
     "Kate, the text editor, with C source code from the kdos-bb demo open.",
     "The Kate editor showing syntax-highlighted C code, with a file tab bar, line numbers and a minimap."),
    ("okular", "desktop/okular.ppm", "Okular",
     "Okular, the document viewer, reading the manual of kdos-doc, which opens the KDOS book that every machine carries.",
     "Okular showing a PDF of the kdos-doc manual page, with page thumbnails in a side bar."),
    ("discover", "desktop/discover.ppm", "Discover",
     "Discover, the software centre, which installs applications and system updates.",
     "The Discover window with categories down the left and seven Editor's Choice applications on its home page, among them KStars, Xournal++ and Meld."),
    ("discover-app", "desktop/discover-app.ppm", "An application in Discover",
     "Discover also offers applications KDOS does not build itself, such as Scribus, built from Debian's packages into a box of their own.",
     "Discover showing Scribus's page: an Install from Debian, built in a box button, its size and licence, a note that it is built on this computer from Debian packages in a box of its own, and its description."),
    ("settings-look", "desktop/settings-look.ppm", "KDOS Look",
     "The KDOS Look page in System Settings picks an accent and turns Phosphor mode on.",
     "System Settings on the KDOS Look page: eight accent swatches named Phosphor, Amber, Ice, Bone, Norton, Borland, Perfect and Paper, and the Phosphor mode switch with its presets."),
] + [accent_shot(a) for a in accents] + [
    ("phosphor-mode", "accents/phosphor-mode.ppm", "Phosphor mode",
     "Phosphor mode draws the whole screen as a CRT, here in the Phosphor accent with scanlines and curved glass.",
     "The desktop drawn as a green-tinted CRT with scanlines and curved edges, showing Dolphin and a Konsole window running fastfetch."),
    ("desk-dock", "accents/desk-dock.ppm", "The Dock desk",
     "Dock, one of the three desks (panel layouts), puts a bar at the top and a floating dock at the bottom.",
     "The desktop with a thin bar along the top and a small floating dock of application icons centred at the bottom, the penguin wallpaper between them."),
    ("desk-tiling", "accents/desk-tiling.ppm", "The Tiling desk",
     "Tiling, another desk, lays windows out side by side by itself, under a bar along the top.",
     "Three windows tiled to fill the screen under a thin bar along the top: Konsole running htop on the left, Dolphin and Kate on the right."),
    ("kdos-command", "desktop/kdos-command.ppm", "The kdos command",
     "The kdos command reports the release, the accents and the state of the system, and changes them.",
     "Konsole showing the output of kdos version, kdos theme list and kdos status."),
    ("console", "console/console.ppm", "The text console",
     "The text console greets a login with the logo and the system's details in a large retro font.",
     "A full-screen text console: a login, then a large KDOS logo and a pixel penguin beside system details in a bold bitmap font."),
    ANIM,
    ("kdos-bb-face", "console/face-004.ppm", "kdos-bb's text portraits",
     "kdos-bb draws portraits of its authors in nothing but text characters.",
     "A portrait of a man's face drawn in ASCII characters on a black terminal, with the start of his biography typed below it."),
    ("limine", "boot/limine.ppm", "The boot menu",
     "The live medium's boot menu, counting down to the live session.",
     "A black boot menu with the KDOS logo at the top and four entries: KDOS Live, KDOS Live (clean session), KDOS Live (verbose) and Memory Test (memtest86+)."),
    ("splash", "boot/splash.ppm", "The boot splash",
     "The boot splash names each service as it starts, under a progress bar.",
     "The boot splash: the penguin and a large pixel KDOS logo, the words Kool Desktop Operating System 0.2, a progress bar and lines naming services with OK beside them."),
    ("greeter", "installed/greeter.ppm", "The login screen",
     "The login screen of an installed system, here wearing the Phosphor accent chosen at install time.",
     "The SDDM login screen in green Phosphor colours: a large clock and date, the account KDOS Test with a password field, and a row of power buttons over the blurred penguin wallpaper."),
    ("lock-screen", "accents/lock-screen.ppm", "The lock screen",
     "The lock screen, with the clock over a blurred wallpaper.",
     "The lock screen: a large clock and date, the user's name and a password field over a blurred penguin wallpaper."),
    ("welcome", "desktop/welcome.ppm", "Welcome Center, at first login",
     "Welcome Center opens on the first login; on the live system its first page starts the installer.",
     "The Welcome Center window over the desktop, with a large Install KDOS button and Skip and Next buttons."),
    ("installer-welcome", "installer/installer-welcome.ppm", "The installer",
     "The installer's first page shows what it found: processor, memory, firmware, disks and the install size.",
     "The Install KDOS window on its Welcome page, with steps down the left and the computer's details on the right."),
    ("installer-disk", "installer/installer-disk.ppm", "Choosing a disk",
     "The Disk page erases a disk or reuses partitions, with two system slots so that an update can be undone.",
     "The installer's Disk page listing one disk, with the option to erase it and install KDOS and a Two system slots check box."),
    ("installer-look", "installer/installer-look.ppm", "Choosing a look",
     "The Look page picks the accent and the desk before the first login.",
     "The installer's Look page with eight accent previews and small drawings of the three desks."),
]
# The animation's frames, the burst's interval, its width and its quality:
# text in motion is the costliest thing WebP compresses, so it is cut to the
# opening titles and drawn at 800 pixels, about 1.5 MB. Its thumbnail is one
# still frame, the BB title, so the one alt text says what the still shows and
# what the rest of the animation spells; the same frame at the animation's own
# width, <name>-still.webp, is what the lightbox opens on, so Play swaps it for
# a picture of the same size.
ANIM_SPEC = ("console/bb-%03d.ppm", range(7, 31), 500, 800, 50, 19,
             "kdos-bb, a 1997 text-mode demo",
             "kdos-bb, a text-mode demo from 1997 that ships with KDOS, playing its opening titles in Konsole.",
             "Two large letters B drawn in ASCII characters on a black terminal, from the opening titles of the kdos-bb demo, which also spell out AA presents, portable, demo, full and mode.")


def lit(im):
    """A black frame, or QEMU's "Display output is not active", is no picture.

    Both are one colour on all but a few hundred pixels and have fewer than
    three colours. The Limine menu, the sparsest real picture, is one colour on
    99.1% of its pixels and has nearly two hundred, so the bound sits above it.
    """
    colours = im.getcolors(im.width * im.height)
    return len(colours) >= 3 and max(n for n, _ in colours) <= 0.995 * im.width * im.height


def frame(im):
    """The gallery frames every picture at 16:9, so a taller one is cut to it.

    Only the Limine menu is taller: the firmware draws at 1280x800, and the
    menu sits at its top with empty rows below, so the cut is taken from the
    bottom. Left uncut, the frame would band it on both sides in the page's
    own background, light in a light accent.
    """
    h = round(im.width * 9 / 16)
    return im.crop((0, 0, im.width, h)) if im.height > h else im


def thumb(im):
    h = round(im.height * THUMB_W / im.width)
    return im.resize((THUMB_W, h), Image.LANCZOS)


def animation():
    pattern, frames, ms, width, quality, still, title, caption, alt = ANIM_SPEC
    paths = [os.path.join(raw, pattern % i) for i in frames]
    missing = [p for p in paths if not os.path.exists(p)]
    if missing:
        bad.append("%s: %d frames missing, first %s" % (ANIM, len(missing), missing[0]))
        return
    seq = []
    for p in paths:
        im = Image.open(p).convert("RGB")
        seq.append(im.resize((width, round(im.height * width / im.width)), Image.LANCZOS))
    full, small, first = ANIM + ".webp", ANIM + "-%d.webp" % THUMB_W, ANIM + "-still.webp"
    seq[0].save(os.path.join(out, full), "WEBP", save_all=True, append_images=seq[1:],
                duration=ms, loop=0, quality=quality, method=6)
    thumb(Image.open(os.path.join(raw, pattern % still)).convert("RGB")).save(
        os.path.join(out, small), "WEBP", quality=FULL_Q, method=6)
    seq[list(frames).index(still)].save(os.path.join(out, first), "WEBP", quality=FULL_Q,
                                        method=6)
    made.update((full, small, first))
    rows.append((full, small, title, caption, alt))


os.makedirs(out, exist_ok=True)
rows, made, bad = [], set(), []
for shot in SHOTS:
    if shot == ANIM:
        animation()
        continue
    name, src, title, caption, alt = shot
    path = os.path.join(raw, src)
    if not os.path.exists(path):
        bad.append("%s: no %s" % (name, path))
        continue
    im = Image.open(path).convert("RGB")
    if not lit(im):
        bad.append("%s: %s is blank" % (name, path))
        continue
    im = frame(im)
    full, small = name + ".webp", name + "-%d.webp" % THUMB_W
    im.save(os.path.join(out, full), "WEBP", quality=FULL_Q, method=6)
    thumb(im).save(os.path.join(out, small), "WEBP", quality=FULL_Q, method=6)
    made.update((full, small))
    rows.append((full, small, title, caption, alt))

# A GALLERY WITH A PICTURE MISSING IS NOT WRITTEN OVER THE LAST WHOLE ONE.
# The pictures that were made are written, but shots.tsv is rewritten, and a
# WebP it does not list removed, only when every picture is there, so a
# conversion without some run's frames leaves the published gallery whole.
total = sum(os.path.getsize(os.path.join(out, f)) for f in made)
print("siteshots: %d pictures, %.1f MB in %s" % (len(rows), total / 1e6, out))
for b in bad:
    print("siteshots: MISSING %s" % b, file=sys.stderr)
if bad:
    print("siteshots: shots.tsv left as it was", file=sys.stderr)
    sys.exit(1)
for f in os.listdir(out):
    if f.endswith(".webp") and f not in made:
        os.remove(os.path.join(out, f))
with open(os.path.join(out, "shots.tsv"), "w") as f:
    f.write("file\tthumb\ttitle\tcaption\talt\n")
    for r in rows:
        assert not any("\t" in c or "\n" in c for c in r), r
        f.write("\t".join(r) + "\n")
PY
}

# ── main ────────────────────────────────────────────────────────────────

if [ "$convert_only" = 0 ]; then
    [ -f build/iso-build/kdos.iso ] || { echo "siteshots: no ISO at build/iso-build/kdos.iso" >&2; exit 1; }
    docker image inspect "$IMAGE" >/dev/null 2>&1 ||
        { echo "siteshots: no rig image $IMAGE; run testing/rig-image.sh" >&2; exit 1; }
    RENDER=$(find /dev/dri -name 'renderD*' -print -quit 2>/dev/null || true)
    for r in "${runs[@]}"; do
        case $r in
            desktop|accents|installer|installed)
                [ -n "$RENDER" ] || { echo "siteshots: the $r run uses --gl, which needs a render node under /dev/dri" >&2; exit 1; } ;;
        esac
    done
    for r in "${runs[@]}"; do
        rm -rf "${RAW:?}/$r"
        mkdir -p "$RAW/$r"
    done
    write_guest_setup
    for r in "${runs[@]}"; do
        "run_$r"
    done
fi
convert
