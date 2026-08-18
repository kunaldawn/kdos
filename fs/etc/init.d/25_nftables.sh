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
        # The file starts with `flush ruleset`, so an error partway through
        # would leave the rules read so far and none of the rest — typically
        # the input chain's drop policy with its accept rules missing, which is
        # a policy nobody wrote. `nft -c` runs the whole file against the
        # kernel and commits nothing, so an unloadable ruleset leaves the
        # previous state standing.
        if ! "$NFT" -c -f "$CONF" 2>/tmp/nft-check.$$; then
            echo "[FAIL] $NAME: $CONF does not load — keeping the current ruleset"
            sed 's/^/       /' /tmp/nft-check.$$
            rm -f /tmp/nft-check.$$
            exit 1
        fi
        rm -f /tmp/nft-check.$$

        echo "[KDOS] Loading $NAME ruleset..."
        "$NFT" -f "$CONF"
        ;;
    stop)
        # `flush ruleset`, not a saved-state restore: the ruleset is a file,
        # so there is no state to save. Stopping the firewall means the machine
        # accepts everything, which is what stopping a firewall is.
        if [ -x "$NFT" ]; then
            echo "[KDOS] Flushing $NAME ruleset..."
            "$NFT" flush ruleset
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
