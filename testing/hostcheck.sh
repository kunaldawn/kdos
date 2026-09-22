#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/hostcheck.sh — the host's own tools, on a booted machine
#
# What a self-test on the build host cannot ask: whether the SHIPPED binary
# under the name a caller resolves is the one that can do the job. Run as
# root IN THE GUEST:
#
#   docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
#       python3 testing/vnc-shot.py --size 1280x800 --wait 30 --no-session \
#       --root-script testing/hostcheck.sh --script-timeout 240
#
# THE blkid BLOCK IS THE REASON THIS FILE EXISTS. Every UUID lookup in the
# initramfs is `blkid -U`, toybox's applet implements no such lookup and
# cannot see `crypto_LUKS`, and $PATH puts /usr/bin ahead of /usr/sbin — so
# the question "which blkid answers" decides whether an installed machine
# boots at all. It is asked here against a real LUKS container because a
# stub cannot answer it.
# ---------------------------------
set -u
rc=0
ok()   { printf '  OK   %s\n' "$1"; }
bad()  { printf '  FAIL %s\n' "$1"; rc=1; }
ck()   { if [ "$2" = "$3" ]; then ok "$1"; else bad "$1: got '$2' want '$3'"; fi; }

echo "== blkid is util-linux's, everywhere =="
W=$(command -v blkid)
echo "  PATH resolves blkid -> $W"
if blkid --version 2>&1 | grep -qi "util-linux"; then ok "PATH blkid is util-linux's"
else bad "PATH blkid is not util-linux's: $(blkid --version 2>&1 | head -1)"; fi
[ -e /usr/bin/blkid ] && bad "/usr/bin/blkid still exists (toybox's name)" \
                      || ok "toybox claims no blkid"

echo
echo "== and it can do what the initramfs asks of it =="
dd if=/dev/zero of=/tmp/v.img bs=1M count=32 2>/dev/null
L=$(losetup -f --show /tmp/v.img)
printf 'pw' | cryptsetup luksFormat --batch-mode --pbkdf pbkdf2 \
    --pbkdf-force-iterations 1000 "$L" - >/dev/null 2>&1
T=$(blkid -s TYPE -o value "$L")
ck "a LUKS container is recognised" "$T" "crypto_LUKS"
U=$(blkid -s UUID -o value "$L")
[ -n "$U" ] && ok "and it has a UUID: $U" || bad "no UUID"
D=$(blkid -U "$U")
ck "-U resolves that UUID back to the device" "$D" "$L"
losetup -d "$L" 2>/dev/null

echo
echo "== the new ports are here and answer =="
# ON $PATH. fprintd is NOT among these — it is a D-Bus activated daemon in
# libexec and is checked by path below, which is the distinction a
# `command -v` sweep gets wrong.
for p in wireplumber wpctl mdadm hdparm thermald openconnect spd-say tpm2 thin_check; do
    if command -v "$p" >/dev/null 2>&1; then ok "$p"; else bad "$p missing"; fi
done
[ -x /usr/libexec/fprintd ] && ok "fprintd (libexec daemon)" || bad "fprintd missing"
[ -e /usr/lib/security/pam_fprintd.so ] && ok "pam_fprintd" || bad "pam_fprintd missing"

echo
echo "== and something actually STARTS the session manager =="
# PRESENCE IS NOT FUNCTION. An image can carry wireplumber and never run it:
# pipewire builds no session manager of its own, so a session script still
# naming the old `pipewire-media-session` starts NOTHING — a graph with
# nothing connected to anything, which is a machine with working hardware
# and no sound. That failure is invisible to "is the binary installed".
S=/usr/local/lib/kdos/session-common.sh
if grep -q "wireplumber" "$S" 2>/dev/null; then ok "the session script starts wireplumber"
else bad "the session script does not start wireplumber"; fi
if grep -q "pipewire-media-session" "$S" 2>/dev/null; then
    bad "the session script still calls pipewire-media-session, which is not built"
else ok "and names no session manager that is not built"; fi
# The same question asked of the whole image rather than one file.
if grep -rl "pipewire-media-session" /usr/local /etc 2>/dev/null | head -1 | grep -q .; then
    bad "something on the image still references pipewire-media-session"
else ok "nothing on the image references the removed session manager"; fi

echo
echo "== and the session manager is protected from the OOM killer =="
# `comm` is truncated at 15 characters and kdos-oomd matches the `pipewire`
# PREFIX; `wireplumber` shares none of it, so it has to be named outright.
if strings -a /usr/sbin/kdos-oomd 2>/dev/null | grep -q wireplumber; then
    ok "kdos-oomd names wireplumber"
else bad "kdos-oomd does not protect wireplumber — audio dies on memory pressure"; fi

echo
echo "== thermald has something to start it =="
[ -x /etc/init.d/54_thermald.sh ] && ok "54_thermald.sh is installed" \
                                  || bad "thermald is installed with no init script"

echo
echo "== the emoji face resolves =="
E=$(fc-match emoji 2>/dev/null)
echo "  fc-match emoji -> $E"
case "$E" in *NotoColorEmoji*) ok "emoji resolves to Noto Color Emoji" ;;
             *) bad "emoji does not resolve to the colour face" ;; esac

echo
echo "== the bluetooth codecs are linked into pipewire =="
N=$(ls /usr/lib/spa-0.2/bluez5/ 2>/dev/null | grep -c "codec")
echo "  codec plugins: $(ls /usr/lib/spa-0.2/bluez5/ 2>/dev/null | grep codec | sed 's/libspa-codec-bluez5-//;s/\.so//' | tr '\n' ' ')"
[ "$N" -ge 8 ] && ok "$N codec plugins" || bad "only $N codec plugins"

echo
echo "== ssh security keys =="
if readelf -d /usr/lib/openssh/ssh-sk-helper 2>/dev/null | grep -q libfido2; then
    ok "ssh-sk-helper links libfido2"
else bad "ssh-sk-helper has no libfido2"; fi

echo
echo "== LVM can reach thin_check where its config says =="
ck "thin_check at the path lvm.conf names" "$([ -x /usr/sbin/thin_check ] && echo yes)" "yes"

echo
[ $rc -eq 0 ] && echo "VERIFY DONE: all checks passed" || echo "VERIFY DONE: FAILURES above"
exit $rc
