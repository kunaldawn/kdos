## About
Jansson is a C library for encoding, decoding and manipulating JSON data, with a small API and no
dependencies of its own. [nftables](#port-nftables) uses it for
`nft --json`, and [networkmanager](#port-networkmanager), [samba](#port-samba), [mtr](#port-mtr), [ddcutil](#port-ddcutil),
[obs-studio](#port-obs-studio) and [handbrake](#port-handbrake) link it too.

## Notes
The recipe has no `depends` line: Jansson needs nothing beyond the C library and the toolchain, and
its configure is the release's own, so no autotools run.
