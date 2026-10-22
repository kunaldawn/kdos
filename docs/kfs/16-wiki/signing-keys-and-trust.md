# Signing keys and trust

A KDOS machine decides whom to believe with directories of public keys, one directory for each kind
of thing it accepts from elsewhere: prebuilt host packages, application packs, and the ports tree
that `kdos update fetch` installs. TLS certificate authorities are a separate store again. This
page covers where each [keyring]{.idx} is, what the image puts in it, how to make a key and sign
with it, how to trust or stop trusting one, and how to read a refusal.

## Installation

The keys are files; the programs that read and write them are on every image:

| Program | Provided by | Uses |
|:--|:--|:--|
| `kpkg keygen`, `sign`, `index --sign`, `verify-index`, `verify-pkg`, `binhost` | kpkg, compiled by `10_bootstrap`; see [kpkg](../03-bootstrap/120-kpkg.md) | [`/etc/kdos/keys`]{.idx} |
| `kdos-pack keygen`, `sign`, `index --sign`, `verify` | [kdos-pack](#port-kdos-pack) | [`/etc/kdos/keys/packs`]{.idx} |
| [`kdos-packd`]{.idx}, which checks a pack before it is installed | [kdos-packd](#port-kdos-packd) | `/etc/kdos/keys/packs` |
| `kdos update fetch` | [kdos-tools](#port-kdos-tools) | [`/etc/kdos/keys/tree`]{.idx} |
| [`update-ca-certificates`]{.idx}, the p11-kit trust module | [ca-certificates](#port-ca-certificates), [p11-kit](#port-p11-kit) | [`/etc/ca-certificates/trust-source/anchors`]{.idx} |

Signatures are Ed25519, made and checked by KDOS's own `libksig`; see
[Signing](../../kdos/03-architecture/packaging.md#signing).

What the image ships in each ring:

| Ring | Shipped key | From |
|:--|:--|:--|
| `/etc/kdos/keys` | None | `fs/etc/kdos/keys/` holds only a `README` |
| `/etc/kdos/keys/packs` | `kdos-packs.pub` | `fs/etc/kdos/keys/packs/` |
| `/etc/kdos/keys/tree` | `kdos-release.pub`, when the build had a release key; otherwise none | `70_image`'s `105_ports.sh` |

The live medium and an installed system carry the same rings; the installer copies them with the
rest of the root.

## Configuration

### /etc/kdos/keys/

The host ring, for a [binary host]{.idx}'s index and for a package's own `.sig` file. Its `README`, from
`fs/etc/kdos/keys/README`, states the policy:

<!-- kfs:include fs/etc/kdos/keys/README -->

Every `*.pub` directly in the directory is trusted; the loader does not descend into `packs/` or
`tree/`, which is what keeps the three rings apart. `KPKG_KEYRING` names another directory for one
[`kpkg`]{.idx} command.

### /etc/kdos/keys/packs/

The ring `kdos-packd` and `kdos-pack verify` check application packs and pack indexes against.
`kdos-pack verify --keys DIR`, or `KDOS_KEYS`, points [`kdos-pack`]{.idx} at another; `kdos-packd` checks
against this directory only. The shipped key, from
`fs/etc/kdos/keys/packs/kdos-packs.pub`:

<!-- kfs:include fs/etc/kdos/keys/packs/kdos-packs.pub -->

A public key file is one line: `kdos-pubkey-1`, the key in hex, and a name. What this key asserts
is in [Signing and trust](../../kdos/03-architecture/security-model.md#signing-and-trust).

### /etc/kdos/keys/tree/

The ring `kdos update fetch` checks a published ports tree's `kdos-ports.tar.xz.sig` against. Its
`README`, from `fs/etc/kdos/keys/tree/README`:

<!-- kfs:include fs/etc/kdos/keys/tree/README -->

The image step that fills it, from `script/phases/70_image/105_ports.sh`:

<!-- kfs:include script/phases/70_image/105_ports.sh lines=132-141 pin=68e72034c604 -->

Where `fetch` looks for the tree is [`/etc/kdos/update.conf`]{.idx}, which is not shipped; see
[/etc/kdos/update.conf](../../kdos/06-reference/configuration.md#etckdosupdateconf).

### Environment variables

| Variable | Read by | Does |
|:--|:--|:--|
| `KPKG_REQUIRE_SIG=1` | `kpkg` ([`kpkgadd`]{.idx}) | Refuses to install a package with no valid `.sig` beside it |
| `KPKG_KEYRING` | `kpkg` | Replaces `/etc/kdos/keys` |
| `KDOS_REQUIRE_SIG` | `kdos-pack verify`, from the caller; `kdos-packd`, from its own environment | Treats an unsigned pack as a failure |
| `KDOS_KEYS` | `kdos-pack` | Replaces `/etc/kdos/keys/packs`. `kdos-packd` reads it only for its status listing; its checks before a mount or an install always use `/etc/kdos/keys/packs` |

`kdos-packd` is started by [`/etc/init.d/59_packd.sh`]{.idx}, which sets no variable before its `supervise`
line:

<!-- kfs:include fs/etc/init.d/59_packd.sh lines=25-26 pin=fca9ddd565f3 -->

### TLS trust anchors

Certificate authorities for TLS are not in any of these rings. The Mozilla bundle is
`/usr/share/ca-certificates/mozilla.pem`, your own roots go in
`/etc/ca-certificates/trust-source/anchors/`, and `update-ca-certificates` writes the combined
[`/etc/ssl/cert.pem`]{.idx}. Which library reads which file is in
[TLS trust anchors](../../kdos/03-architecture/security-model.md#tls-trust-anchors).

## Usage

### Listing what this machine trusts

```bash
ls /etc/kdos/keys /etc/kdos/keys/packs /etc/kdos/keys/tree
cat /etc/kdos/keys/packs/*.pub
```

Each `.pub` is one trusted key. `kpkg` and `kdos-pack` print a key's 16-character id when they make
or verify with it; the id names a key for a person and grants nothing.

### Trusting a binary host

On the machine that builds:

1. Make a key pair, once:

   ```bash
   kpkg keygen builder
   ```

   It writes `builder.key`, the secret half, mode 0600, and `builder.pub`, and prints the key's id
   (`==> Key <id>`). An existing `builder.key` is never overwritten: a second `keygen builder`
   ends `ERROR: cannot write builder.key (it must not already exist)`.

2. Index and sign a directory of packages:

   ```bash
   kpkg index /repo --sign builder.key
   ```

   It writes `PACKAGES`, `PACKAGES.sig`, and a `.sig` beside every package, and reports
   `Indexed N package(s)`, `Signed with key <id>` and `Wrote N package signature(s)`.

On every machine that should use it:

3. Copy the public half in:

   ```bash
   sudo cp builder.pub /etc/kdos/keys/
   ```

4. Check the index, then install from it:

   ```bash
   kpkg verify-index /repo
   sudo kpkg binhost /repo zlib
   ```

   `verify-index` prints `signature good (key <id>)` and the id of the key that verified, which may
   differ from the id the signature names. The three tests `binhost` makes before it uses a package
   are in [The binary host](../../kdos/03-architecture/packaging.md#the-binary-host); building one
   with the image is on [The binary host](../15-image/010-binhost.md).

### Signing one package

For a package that travels without an index:

```bash
kpkg sign <package>.tar.xz builder.key    # writes <package>.tar.xz.sig
kpkg verify-pkg <package>.tar.xz
```

`verify-pkg` prints `signature good (key <id>)`, or, for a file changed after it was signed or a
key not in the ring, `ERROR: <package>.tar.xz: NO trusted signature` and exit status 1.

`kpkgadd` checks the `.sig` when it finds one, and refuses the package when it does not verify. A
package with no `.sig` is installed, unless `KPKG_REQUIRE_SIG=1` is set.

### Signing packs with your own key

1. Make a pack key, on the machine that makes packs:

   ```bash
   kdos-pack keygen mypacks
   ```

   It writes `mypacks.key` (0600) and `mypacks.pub`, and prints the key's id beside the public
   file's name (`mypacks.pub  <id>`).

2. Sign a pack, or a directory of packs and its index:

   ```bash
   kdos-pack sign app.kpack mypacks.key
   kdos-pack index ./packs --sign mypacks.key
   ```

   `sign` prints `app.kpack signed`, and `index` prints `./packs/PACKAGES  N entries, signed`.

3. On every machine that should accept them, put the public half in the pack ring. To trust only
   your packs, remove the shipped key as well:

   ```bash
   sudo cp mypacks.pub /etc/kdos/keys/packs/
   sudo rm /etc/kdos/keys/packs/kdos-packs.pub     # only if the medium's packs are not wanted
   ```

4. Check a pack against the ring:

   ```bash
   kdos-pack verify app.kpack
   ```

   It prints `<pack id>: signed by <key id>`, `<pack id>: unsigned`, or the reason the signature
   failed. A signature by a key that is not in the ring reads `bad signature`, the same as a pack
   changed after it was signed, and exits 2; `--keys DIR` or `KDOS_KEYS` names a directory to try
   instead. With `KDOS_REQUIRE_SIG=1` an unsigned pack reads
   `unsigned, and KDOS_REQUIRE_SIG is set` and exits 2.

How packs are built and composed is in
[Building a pack](../../kdos/03-architecture/packs-and-boxes.md#building-a-pack).

### Refusing unsigned packs

1. As root, edit `/etc/init.d/59_packd.sh` and add `export KDOS_REQUIRE_SIG=1` on the line before
   `supervise "$NAME" "$DAEMON"`.
2. Restart the daemon: `sudo service restart packd`.
3. From then on `kdos-packd` refuses an unsigned pack at install. A pack mounted straight from a
   medium is not covered; see
   [How an application is verified](../../kdos/03-architecture/security-model.md#how-an-application-is-verified).

Not verified: the image carries no pack on its medium, so no import has been refused this way on
a booted image. `kdos-pack verify` with `KDOS_REQUIRE_SIG=1` shows the same check.

### Following a ports tree you publish

1. Replace the tree ring's key with the public half of the key you sign trees with:

   ```bash
   sudo rm /etc/kdos/keys/tree/*.pub
   sudo cp mytree.pub /etc/kdos/keys/tree/
   ```

2. Point `fetch` at where you publish, with `tree =` in `/etc/kdos/update.conf`, or once:

   ```bash
   sudo kdos update fetch --from /media/stick/release
   ```

   A directory or URL must hold `kdos-ports.tar.xz` and `kdos-ports.tar.xz.sig`.

Not verified: these steps have not been run against a published, signed tree; see Known gaps.

### Trusting a local certificate authority

```bash
sudo cp root.crt /etc/ca-certificates/trust-source/anchors/   # PEM only
sudo update-ca-certificates
```

Firefox, LibreWolf and Thunderbird keep their own store and need the root added in their own
settings.

### Removing trust

Delete the key's `.pub` from its ring. There is no revocation list and no cache: every
verification reads the ring afresh, so the next one does not try the deleted key.

## Tips

- Keep every `.key` file off the machines that only verify. A secret key is needed only where you
  sign.
- A build keeps its keys in `build/keys/`, which `make clean` and `make cleanbuild` leave in place.
  Without the pack key, later bakes are unsigned, while media already written keep trusting a key
  nobody can sign with.
- A key placed in the wrong ring is trusted only for that ring's kind of file. A pack key copied
  into `/etc/kdos/keys` would make its holder a trusted source of host packages.
- `kdos explain libksig` prints the design note on why a key id is not a key.

## Troubleshooting

**`kpkg verify-index` or `kpkg binhost` prints `no trusted keys in /etc/kdos/keys`.**
The ring is empty, as it ships. Copy the builder's `.pub` in. `kpkg binhost` exits 2 for this, for
a missing index, and for a failed verification.

**`kpkg binhost` prints `no trusted signature — refusing to use it`.**
`PACKAGES.sig` was made by a key that is not in `/etc/kdos/keys`, or the index changed after it
was signed; `binhost` exits 2. Run `kpkg verify-index <dir>` to see which. Re-index with `--sign` on the build machine
after changing anything in the directory.

**`kpkg binhost` exits 1 and says `build from source`.**
The binhost has no such package, or its architecture, build configuration or recipe hash differs
from this machine's; the message says which. That is a match failure, not a trust failure, and
`binhost` installs nothing: build the port with `kpkg install`.

**`kdos update check` reports `fetch-failed`.**
Run `sudo kdos update fetch` to see the reason. With no `/etc/kdos/update.conf` it downloads from
the project's latest release, and while none carries a tree it ends
`kdos update: https://github.com/kunaldawn/kdos/releases/latest/download/kdos-ports.tar.xz could not be downloaded`,
after curl's `The requested URL returned error: 404`. A tree that does download is checked against
the ring next: an image built without a release key has an empty `/etc/kdos/keys/tree` and refuses
every tree with `no trusted tree key in /etc/kdos/keys/tree`. A tree whose signature does not
verify, or whose `TREE` stamp is older than the installed one, is refused. The states are in
[kdos update](../../kdos/04-programs/kdos-command.md#kdos-update).

**A pack import is refused.**
Read the daemon's log, `/run/kdos-svc.kdos-packd.log`. A bad payload hash, a bad signature, or a
signature by a key that is not in `/etc/kdos/keys/packs` is refused; `kdos-pack verify <pack>`
reports the same state without installing.

**`curl` accepts a local root and a GnuTLS program does not, or the reverse.**
The two sides read different files. `update-ca-certificates` updates OpenSSL's `/etc/ssl/cert.pem`
and reports `/etc/ssl/cert.pem: the Mozilla bundle and N local anchor file(s)`; GnuTLS reads the
anchors directory through p11-kit at once, so `trust list` shows a new root before
`update-ca-certificates` has run. A DER file is skipped by `update-ca-certificates`, which says
`<file> holds no PEM certificate, skipped`; convert it with
`openssl x509 -in root.der -inform der -out root.crt`.

## Known gaps

- [There is no public binary host](../../kdos/06-reference/known-gaps.md#there-is-no-public-binary-host)
- [The update path has not been run against a published tree](../../kdos/06-reference/known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree)
- [Secure Boot is not supported](../../kdos/06-reference/known-gaps.md#secure-boot-is-not-supported):
  the boot loader is unsigned
- Unsigned packs and unenforced module signatures, among the points in
  [Security](../../kdos/06-reference/known-gaps.md#security)

## See also

- [Signing and trust](../../kdos/03-architecture/security-model.md#signing-and-trust): the three
  rings and what each key asserts
- [Signing](../../kdos/03-architecture/packaging.md#signing) and
  [The binary host](../../kdos/03-architecture/packaging.md#the-binary-host): the package side
- [How an application is verified](../../kdos/03-architecture/security-model.md#how-an-application-is-verified):
  the checks `kdos-packd` makes
- [Module signing](../../kdos/03-architecture/security-model.md#module-signing): the kernel's own key
- [/etc/kdos/keys/](../../kdos/06-reference/configuration.md#etckdoskeys): the reference
- [The binary host](../15-image/010-binhost.md), [The recipe tree and the source offer](../15-image/105-ports.md),
  [kpkg](kpkg.md), [Packs](packs.md), [Updating](updating.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 386.* Previous: [385. SSH server](ssh-server.md) · [Contents](../README.md) · Next: [387. NetworkManager](networkmanager.md)
