#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/flows/lib.sh — what every login flow shares, sourced by each
#
# A flow is ONE BOOT driven by one vnc-shot.py run: its steps (keys, typing,
# --root-script assertions and shots) are one ordered list, so the state a
# step asserts is the state the step before it left. Every assertion is
# testing/flows/probe.sh run in the guest; vnc-shot.py exits 3 when one of
# them failed and non-zero when its readiness gate did, and the flow exits
# with that status. The shots are for the person reading a failure, never
# evidence of a pass.
#
#   KDOS_FLOW_PASSWORD  the account's password (kdos: the live medium's, and
#                       the one testing/install-to-disk.sh sets)
#   KDOS_FLOW_DISK      the installed disk the greeter flows boot
#                       (build/kdos-target.qcow2, from install-to-disk.sh with
#                       its default answers: no autologin). Never written: a
#                       flow boots a qcow2 overlay of it, removed afterwards
#   KDOS_FLOW_SESSION_VT  the VT the autologin session runs on (1)
#   KDOS_FLOW_DEV       extra docker arguments, e.g. a /dev/dri for --gl
#
# A flow that sets FLOW_LIT before flow_run has those shots read for a lit
# screen (flow_lit) once every probe has passed.
#
# Shots and the serial log land in build/shots/flows/<flow>/.
# ---------------------------------

cd "$(dirname "$0")/../.." || exit 1

# The four the flow that sources this reads.
# shellcheck disable=SC2034
FLOW_PASSWORD=${KDOS_FLOW_PASSWORD:-kdos}
FLOW_DISK=${KDOS_FLOW_DISK:-build/kdos-target.qcow2}
# shellcheck disable=SC2034
FLOW_VT=${KDOS_FLOW_SESSION_VT:-1}
# shellcheck disable=SC2034
PROBE=testing/flows/probe.sh

# flow_init NAME — the flow's shot directory, and the ISO it needs.
flow_init() {
	FLOW=$1
	FLOW_LIT=""
	SHOTS=build/shots/flows/$FLOW
	# shellcheck disable=SC2034 # the flow's --shot paths, inside the container
	SHOT=/kdos/$SHOTS
	mkdir -p "$SHOTS"
	rm -f "$SHOTS"/*.ppm
	OVERLAY=""
	[ -f build/iso-build/kdos.iso ] ||
		{ echo "flow $FLOW: no build/iso-build/kdos.iso — run a full build once" >&2; exit 1; }
}

# flow_disk — a throwaway overlay of the installed disk, so a flow that logs
# in, writes a session log and logs out leaves the base disk as it was.
flow_disk() {
	[ -f "$FLOW_DISK" ] || {
		echo "flow $FLOW: no installed disk at $FLOW_DISK — make one with" >&2
		echo "  testing/vnc-shot.py --disk $FLOW_DISK --no-session --root-script testing/install-to-disk.sh" >&2
		exit 1
	}
	OVERLAY=build/shots/flows/$FLOW/disk.qcow2
	rm -f "$OVERLAY"
	docker run --rm -v "$PWD:/kdos" -w /kdos kdos-qemu-py:latest \
		qemu-img create -q -f qcow2 -F qcow2 -b "/kdos/$FLOW_DISK" "/kdos/$OVERLAY" ||
		{ echo "flow $FLOW: qemu-img could not make the overlay" >&2; exit 1; }
}

# flow_run ARGS... — boot and run the steps; prints the verdict and returns
# vnc-shot.py's status, or 4 when it passed and a shot FLOW_LIT names (file
# names in the flow's shot directory) is unlit (flow_lit).
flow_run() {
	rc=0
	# shellcheck disable=SC2086 # KDOS_FLOW_DEV is a list of docker arguments
	docker run --rm --device /dev/kvm ${KDOS_FLOW_DEV:-} -v "$PWD:/kdos" -w /kdos \
		kdos-qemu-py:latest python3 testing/vnc-shot.py --size 1920x1080 \
		--serial-log "/kdos/$SHOTS/serial.log" "$@" || rc=$?
	[ -n "$OVERLAY" ] && rm -f "$OVERLAY"
	if [ "$rc" -eq 0 ] && [ -n "${FLOW_LIT:-}" ]; then
		lit=""
		for f in $FLOW_LIT; do
			lit="$lit $SHOTS/$f"
		done
		# shellcheck disable=SC2086 # the shot paths have no spaces
		flow_lit $lit || rc=4
	fi
	if [ "$rc" -eq 0 ]; then
		echo "flow $FLOW: PASS (shots in $SHOTS)"
	elif [ "$rc" -eq 4 ]; then
		echo "flow $FLOW: FAIL, a screen the probes passed is unlit (shots and serial.log in $SHOTS)"
	else
		echo "flow $FLOW: FAIL, vnc-shot.py exited $rc (shots and serial.log in $SHOTS)"
	fi
	return "$rc"
}

# flow_lit SHOT... — fail, naming each, the shots whose screen is unlit. A
# flow's probes ask logind, the bus and the kernel, and all three report a
# healthy session while QEMU's scanout is inactive (the guest's DRM state
# still reads the output On), so only the host's frame shows a screen that
# never came back. A frame is unlit when a 64x36 grid of its pixels holds
# fewer than 16 colours or one colour on more than 98% of the points: QEMU's
# "Display output is not active" frame and a black screen both are; the lock
# screen and the desktop hold about a hundred. A text console is unlit by
# this measure too, so it is for shots of the graphical session only.
flow_lit() {
	python3 -I - "$@" <<'PY'
import collections, re, sys

HEADER = re.compile(rb"P6(?:\s|#[^\n]*\n)+(\d+)(?:\s|#[^\n]*\n)+(\d+)"
                    rb"(?:\s|#[^\n]*\n)+255\s")
bad = 0
for path in sys.argv[1:]:
    try:
        with open(path, "rb") as f:
            data = f.read()
        m = HEADER.match(data)
        if not m:
            raise ValueError("not an 8-bit P6 PPM")
        w, h = int(m.group(1)), int(m.group(2))
        pos = m.end()
        if len(data) < pos + 3 * w * h:
            raise ValueError("shorter than its %dx%d header" % (w, h))
        seen = collections.Counter()
        for gy in range(36):
            for gx in range(64):
                o = pos + 3 * ((gy * h // 36 + h // 72) * w
                               + gx * w // 64 + w // 128)
                seen[data[o:o + 3]] += 1
    except (OSError, ValueError) as e:
        print("flow_lit: %s: unreadable: %s" % (path, e))
        bad = 1
        continue
    top = seen.most_common(1)[0][1] / (64 * 36)
    if len(seen) < 16 or top > 0.98:
        print("flow_lit: %s: UNLIT, %d colours, %.0f%% one colour"
              % (path, len(seen), top * 100))
        bad = 1
    else:
        print("flow_lit: %s: lit, %d colours" % (path, len(seen)))
sys.exit(bad)
PY
}
