## About

vpnc-script is the script openconnect runs when a tunnel comes up or goes down: it sets
the tunnel's addresses and routes with `ip` and writes its DNS servers. The port installs it as
`/usr/share/vpnc-scripts/vpnc-script`, the path openconnect is built to call.

## Notes

The source is that one script, fetched from a pinned commit. Its header puts it under the GPL-2.0
or any later version and does not include the licence text, so kpkg finds none. The last line of
`build.sh` installs iproute2's `/usr/share/licenses/iproute2/COPYING`, the GPL-2.0 text, which is
there because `iproute2` is in `depends`.
