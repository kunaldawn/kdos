#!/bin/bash
. /etc/init.d/service_helper

NAME="zram"
CONF="/etc/kdos/zram.conf"
DEV="/dev/zram0"
SYS="/sys/block/zram0"

# NOT A SUPERVISED SERVICE. Setting up a compressed swap device is three writes
# to sysfs and a swapon; the kernel then holds it. Under `supervise` this would
# be a respawn loop around a program that is supposed to exit — 25_nftables.sh
# is the same shape for the same reason.
#
# 12, so it runs after 02_modules and well before anything that allocates:
# swap that arrives late is swap the machine has already gone without.

# `size` is a PERCENTAGE OF RAM, not a byte count, because the compressed pages
# live in that same RAM: a zram device is a promise to store `size` bytes of
# swap in rather less physical memory, and how much less depends on what is in
# it. 50 is the conventional figure and survives a compression ratio of 2:1
# without the device being able to consume the machine.
size=50
algorithm=zstd

conf_key() {
    [ -r "$CONF" ] || return 0
    sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*\([A-Za-z0-9]\{1,\}\).*/\1/p" \
        "$CONF" | tail -1
}

case "$1" in
    start)
        # THE MODULE IS CHECKED BEFORE ANYTHING ELSE IS TOUCHED. CONFIG_ZRAM=m,
        # so a kernel built without it, or one whose modules were not installed,
        # must leave this a skip rather than a failure — a machine with no zram
        # is a machine with no zram, not a broken boot.
        if [ ! -d "$SYS" ] && ! modprobe zram 2>/dev/null; then
            echo "[SKIP] $NAME: the zram module will not load"
            exit 0
        fi
        [ -d "$SYS" ] || { echo "[SKIP] $NAME: $SYS did not appear"; exit 0; }

        if grep -q "^$DEV " /proc/swaps 2>/dev/null; then
            echo "[KDOS] $NAME: $DEV is already swap"
            exit 0
        fi

        _s=$(conf_key size);      case "$_s" in ''|*[!0-9]*) ;; *) size=$_s ;; esac
        _a=$(conf_key algorithm); [ -n "$_a" ] && algorithm=$_a
        if [ "$size" -lt 1 ] || [ "$size" -gt 90 ]; then
            echo "[KDOS] $NAME: size $size is outside 1-90, using 50"
            size=50
        fi

        # An algorithm the kernel does not carry is REPORTED and the kernel's
        # own default is kept. Writing it anyway leaves the device at its
        # default silently, so the log would name a compressor that is not the
        # one doing the work.
        if [ -r "$SYS/comp_algorithm" ]; then
            if grep -qw "$algorithm" "$SYS/comp_algorithm"; then
                echo "$algorithm" > "$SYS/comp_algorithm"
            else
                echo "[KDOS] $NAME: no '$algorithm' backend in this kernel — keeping the default"
                algorithm=$(sed -n 's/.*\[\([a-z0-9]*\)\].*/\1/p' "$SYS/comp_algorithm")
            fi
        fi

        _kb=$(sed -n 's/^MemTotal:[[:space:]]*\([0-9]*\).*/\1/p' /proc/meminfo)
        [ -n "$_kb" ] || { echo "[SKIP] $NAME: cannot read MemTotal"; exit 0; }
        _bytes=$(( _kb * 1024 / 100 * size ))

        echo "$_bytes" > "$SYS/disksize" || {
            echo "[FAIL] $NAME: the kernel refused a ${size}% disksize"
            exit 1
        }

        mkswap "$DEV" >/dev/null 2>&1 || { echo "[FAIL] $NAME: mkswap $DEV"; exit 1; }

        # Priority above any disk swap fstab carries. Swapping to compressed
        # RAM and swapping to a disk are not the same operation at the same
        # cost, and equal priorities would round-robin between them.
        swapon -p 100 "$DEV" || { echo "[FAIL] $NAME: swapon $DEV"; exit 1; }
        echo "[KDOS] $NAME: $DEV, ${size}% of RAM, $algorithm"
        ;;
    stop)
        if grep -q "^$DEV " /proc/swaps 2>/dev/null; then
            echo "[KDOS] Stopping $NAME..."
            swapoff "$DEV" 2>/dev/null
        fi
        [ -w "$SYS/reset" ] && echo 1 > "$SYS/reset" 2>/dev/null
        ;;
    status)
        if [ ! -d "$SYS" ]; then
            echo "$NAME: no zram device"
            exit 1
        fi
        if grep -q "^$DEV " /proc/swaps 2>/dev/null; then
            echo "$NAME: $DEV active, $(cat "$SYS/disksize") bytes nominal"
        else
            echo "$NAME: $DEV present but not swap"
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
