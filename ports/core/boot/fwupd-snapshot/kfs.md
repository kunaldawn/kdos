## About
The LVFS firmware metadata as it stood on the day a KDOS release was cut: the Linux Vendor
Firmware Service's `firmware.xml.zst` and the `firmware.xml.zst.jcat` that signs it, installed
under `/usr/share/fwupd/remotes.d/kdos-snapshot/`. The `kdos-snapshot` local remote,
`/usr/share/fwupd/remotes.d/kdos-snapshot.conf` from `fs/`, reads it, so
[fwupd](#port-fwupd) lists the firmware updates that apply to this machine, in `fwupdmgr
get-updates` and on Discover's Updates page, with no network. Installing one fetches its cabinet
from the LVFS, a download somebody started by pressing the button.

## Notes
- **The source is made, not downloaded by the build.** `ports/fwupd-snapshot`, run when a
  release is cut, fetches the two files, checks them, packs them with a `SNAPSHOT` record (where
  they came from, when, their sha256 and the signing certificate's subject) into
  `fwupd-snapshot-<date>.tar.xz`, and rewrites `version` and `sha256` here. `ports/publish
  fwupd-snapshot` then puts the tarball in the source archive, which is the only place it exists;
  `source =` names the file and no URL. The tarball is reproducible: sorted members, owner 0,
  every mtime the date at 00:00 UTC, so a second run over the same two files gives the same
  hash.
- **The signature is checked before the tarball is written.** fwupd trusts a local remote
  outright, marking every release in it as trusted metadata and payload, so the snapshot must be
  the LVFS's own. The script requires a PKCS#7 signature in the jcat to verify over the metadata
  against the LVFS CA certificates, which it takes from the fwupd source tarball after checking
  that tarball against [fwupd](#port-fwupd)'s pinned sha256, and every SHA-256 and SHA-512 the jcat
  records to match. The LVFS signs with RSA and with ML-DSA-87; `openssl cms` verifies the RSA
  signature and refuses the ML-DSA one's SHAKE256 digest, which the script reports and passes
  over. After that the recipe's sha256 pins the bytes, as it does every source.
- **It is as current as the release.** Firmware published after the snapshot does not appear
  until the next release, or until `fwupdmgr enable-remote lvfs` turns on the LVFS's own
  download remote, which then outranks this one (`OrderAfter=lvfs`) and which Discover refreshes
  at each start.
