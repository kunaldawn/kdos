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
# vnc-shot.py's status.
flow_run() {
	rc=0
	# shellcheck disable=SC2086 # KDOS_FLOW_DEV is a list of docker arguments
	docker run --rm --device /dev/kvm ${KDOS_FLOW_DEV:-} -v "$PWD:/kdos" -w /kdos \
		kdos-qemu-py:latest python3 testing/vnc-shot.py --size 1920x1080 \
		--serial-log "/kdos/$SHOTS/serial.log" "$@" || rc=$?
	[ -n "$OVERLAY" ] && rm -f "$OVERLAY"
	if [ "$rc" -eq 0 ]; then
		echo "flow $FLOW: PASS (shots in $SHOTS)"
	else
		echo "flow $FLOW: FAIL, vnc-shot.py exited $rc (shots and serial.log in $SHOTS)"
	fi
	return "$rc"
}
