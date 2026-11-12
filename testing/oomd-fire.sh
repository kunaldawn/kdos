#!/bin/sh
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/oomd-fire.sh — make kdos-oomd fire, on a real machine
#
# The victim SELECTION is exercised against a recorded /proc tree by
# `kdos-oomd --fixture`; what that cannot show is the daemon waking at all.
# This puts a machine under genuine memory pressure and asks the daemon what
# it did. Run it as root IN THE GUEST:
#
#   docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
#       python3 testing/vnc-shot.py --size 1280x800 --wait 30 --no-session \
#       --root-script testing/oomd-fire.sh --script-timeout 180
#
# WHAT COUNTS AS EVIDENCE is the socket's own `status`: `kills` before and
# after, and the victim it names. A hog that merely died proves nothing —
# the kernel's own OOM killer would also have killed it, later and after the
# desktop had already stopped answering, which is the whole reason this
# daemon exists.
#
# THE MEMORY IS TOUCHED AND NOT MERELY RESERVED. An untouched mapping costs
# no page, raises no pressure and would have the script sit at zero until it
# gave up.
#
# THE SECOND ROUND STALLS BEFORE IT RUNS SHORT. A hog held under a cgroup's
# memory.high reclaims into swap and stalls the whole machine's PSI while
# MemAvailable is still high, so the trigger fires with memory not short; a
# second hog then exhausts memory inside the daemon's ten-second cool-down.
# The daemon must still kill it, and the kernel's OOM count must not move: a
# firing that finds memory plentiful is the one that must not start the
# cool-down.
# ---------------------------------
echo "### kernel"
uname -r
echo "### psi before"
cat /proc/pressure/memory
echo "### memory"
free -m 2>/dev/null || head -3 /proc/meminfo
echo "### swap"
cat /proc/swaps
echo "### daemon"
ps ax 2>/dev/null | grep -c '[k]dos-oomd'
ask() {
    python3 - "$1" <<'PY'
import socket, sys
try:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(3)
    s.connect("/run/kdos-oomd.sock")
    s.sendall(sys.argv[1].encode())
    print(s.recv(512).decode().strip())
except Exception as e:
    print("socket error:", e)
PY
}
oomcount() { dmesg 2>/dev/null | grep -c 'Out of memory'; }
echo "### status before"
ask status
echo "kernel oom kills $(oomcount)"

# THE VICTIM. A clearly-largest RSS, touched rather than merely reserved: an
# untouched mapping costs no page and generates no pressure at all.
echo "### hog"
python3 -c '
import os, time
print("hogpid", os.getpid(), flush=True)
b = []
while True:
    c = bytearray(128 * 1024 * 1024)
    for i in range(0, len(c), 4096):
        c[i] = 1
    b.append(c)
    time.sleep(0.02)
' > /tmp/hog.out 2>/tmp/hog.err &
HOG=$!
sleep 2
HPID=$(head -1 /tmp/hog.out | awk '{print $2}')
echo "shell child $HOG, python pid ${HPID:-unknown}"

i=0
while [ $i -lt 90 ]; do
    if ! kill -0 "$HOG" 2>/dev/null; then
        echo "### hog gone after ${i}s"
        break
    fi
    i=$((i + 1))
    sleep 1
done
[ $i -ge 90 ] && { echo "### hog SURVIVED 90s"; kill -9 "$HOG" 2>/dev/null; }

echo "### psi after"
cat /proc/pressure/memory
echo "### status after"
ask status
echo "### hog stderr"
tail -3 /tmp/hog.err

echo "### stall first"
# The first round's kill started a cool-down of its own; a round inside it
# would measure that one, not the firing with memory plentiful.
sleep 11
CG=/sys/fs/cgroup/kdos-oomd-fire
if mkdir "$CG" 2>/dev/null && echo 64M > "$CG/memory.high"; then
    sh -c "echo \$\$ > $CG/cgroup.procs; exec python3 -c '
import time
b = bytearray(400 * 1024 * 1024)
t = time.time()
while time.time() - t < 6:
    for i in range(0, len(b), 4096):
        b[i] = (b[i] + 1) & 255
'" &
    STALL=$!
    sleep 2
    sed -n 2p /proc/pressure/memory
    awk '/MemAvailable/ { print "available", int($2 / 1024), "MB" }' /proc/meminfo
    python3 -c '
b = []
while True:
    c = bytearray(128 * 1024 * 1024)
    for i in range(0, len(c), 4096):
        c[i] = 1
    b.append(c)
' 2>/dev/null &
    HOG=$!
    i=0
    while kill -0 "$HOG" 2>/dev/null && [ $i -lt 60 ]; do
        i=$((i + 1))
        sleep 1
    done
    kill -9 "$STALL" "$HOG" 2>/dev/null
    wait 2>/dev/null
    sleep 1
    rmdir "$CG"
    echo "### status after stall first"
    ask status
    echo "kernel oom kills $(oomcount)"
else
    echo "no cgroup v2 memory controller; skipped"
fi
echo "### done"
