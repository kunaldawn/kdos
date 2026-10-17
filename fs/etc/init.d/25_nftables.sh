#!/bin/bash
. /etc/init.d/service_helper

NAME="nftables"
NFT="/usr/sbin/nft"
CONF="/etc/nftables.conf"

# NOT A SUPERVISED SERVICE. nft hands a ruleset to the kernel and exits; the
# kernel holds it. Under `supervise` this would be a respawn loop around a
# program that is supposed to exit.
#
# 25, so it runs BEFORE 30_network.sh: a firewall applied after the interface
# is up leaves a window, and that window is the one that matters on a network
# the user did not choose.

# THE FALLBACK WHEN THE RULESET DOES NOT LOAD. At boot nothing is loaded yet,
# so "keep the current ruleset" would mean no input filter at all, while sshd
# and every other listener start. With no `inet filter` table in the kernel,
# this loads the shipped file without its drop-ins, the part an administrator
# edits least; when that does not load either, the minimal drop policy below:
# answers to this machine's own connections, loopback, the ICMP that IPv4 and
# IPv6 need to work, and the DHCPv6 client. Either way the service reports
# [FAIL], since the ruleset that was asked for is not the one running.
FALLBACK=/run/nftables-fallback.nft

load_fallback() {
    if "$NFT" list table inet filter >/dev/null 2>&1; then
        echo "[FAIL] $NAME: $CONF does not load — keeping the current ruleset"
        return
    fi
    grep -v '^[[:space:]]*include[[:space:]]' "$CONF" > "$FALLBACK"
    if "$NFT" -c -f "$FALLBACK" 2>/dev/null && "$NFT" -f "$FALLBACK"; then
        echo "[FAIL] $NAME: $CONF does not load — loaded it without /etc/nftables.d"
        return
    fi
    cat > "$FALLBACK" <<'NFT'
table inet filter
delete table inet filter
table inet filter {
	chain input {
		type filter hook input priority filter; policy drop;
		ct state established,related accept
		ct state invalid drop
		iif lo accept
		ip6 nexthdr icmpv6 icmpv6 type { destination-unreachable, packet-too-big, time-exceeded, parameter-problem, echo-request, nd-router-advert, nd-neighbor-solicit, nd-neighbor-advert } accept
		ip protocol icmp icmp type { destination-unreachable, time-exceeded, parameter-problem, echo-request } accept
		udp dport 546 accept
	}
	chain forward {
		type filter hook forward priority filter; policy drop;
		ct state established,related accept
	}
	chain output {
		type filter hook output priority filter; policy accept;
	}
}
NFT
    if "$NFT" -f "$FALLBACK"; then
        echo "[FAIL] $NAME: $CONF does not load — loaded the minimal drop policy"
    else
        echo "[FAIL] $NAME: no ruleset could be loaded — input is NOT filtered"
    fi
}

case "$1" in
    start)
        if [ ! -x "$NFT" ]; then
            echo "[SKIP] $NAME: $NFT not found"
            exit 0
        fi
        if [ ! -f "$CONF" ]; then
            echo "[SKIP] $NAME: no $CONF — no ruleset to load"
            exit 0
        fi

        # CHECK BEFORE LOADING, AND REFUSE RATHER THAN HALF-APPLY.
        #
        # The file deletes and rebuilds `inet filter`, so an error partway
        # through would leave the rules read so far and none of the rest — typically
        # the input chain's drop policy with its accept rules missing, which is
        # a policy nobody wrote. `nft -c` runs the whole file against the
        # kernel and commits nothing, so an unloadable ruleset leaves a
        # loaded one standing, and load_fallback covers a boot with none.
        if ! "$NFT" -c -f "$CONF" 2>/tmp/nft-check.$$; then
            load_fallback
            sed 's/^/       /' /tmp/nft-check.$$
            rm -f /tmp/nft-check.$$
            exit 1
        fi
        rm -f /tmp/nft-check.$$

        echo "[KDOS] Loading $NAME ruleset..."
        "$NFT" -f "$CONF"
        ;;
    stop)
        # The firewall's own table is deleted, not a saved-state restore: the
        # ruleset is a file, so there is no state to save. Stopping the
        # firewall means the machine accepts everything, which is what stopping
        # a firewall is. Only `inet filter` goes — `flush ruleset` would also
        # take netavark's NAT for running containers and NetworkManager's for
        # the hotspot, which are not this firewall's to remove.
        if [ -x "$NFT" ]; then
            echo "[KDOS] Removing $NAME ruleset..."
            "$NFT" delete table inet filter 2>/dev/null
        fi
        ;;
    status)
        if [ ! -x "$NFT" ]; then
            echo "$NAME: not installed"
            exit 1
        fi
        if "$NFT" list ruleset 2>/dev/null | grep -q 'type filter hook input'; then
            echo "$NAME: ruleset loaded"
        else
            echo "$NAME: no ruleset loaded"
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|status}"
        exit 1
        ;;
esac
