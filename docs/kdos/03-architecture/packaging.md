# Packaging

How host software is described, built, installed, verified and updated. This covers the ports
tree, the package format, reproducibility, the signed binary host, deltas and vulnerability
tracking. For writing a recipe, see [Writing ports](../05-developer/writing-ports.md); for
application packs, which are a different system, see [Packs and boxes](packs-and-boxes.md).

## The ports tree

A **port** is a directory describing one piece of software. It is two files:

| File | What it is |
|---|---|
| `kpkgbuild` | Declarative metadata. **Parsed, never sourced** |
| `build.sh` | The build. Ordinary bash, with the working directory set to the unpacked source |

Optionally beside them: `postinstall.sh`, patch files, and — for a port whose sources are release
assets — the tarballs themselves.

The split is deliberate. Because the metadata is parsed rather than executed, reading a recipe
costs no shell and cannot run anything; and because the build is a real script, syntax checking,
linting, highlighting and diffing all work on it. See
[Decisions](../01-philosophy/decisions.md).

**There are three port repositories**, searched in order, and they use one format:

```
PORT_REPO="/ports/core /kdos/src/packages /kdos/src/desktop"
```

`ports/core` is upstream software. `src/packages` and `src/desktop` are ours. Building the
desktop is therefore not a special case anywhere in the build system.

## kpkg

One binary answering to five names, dispatched on its own basename:

| Name | Does |
|---|---|
| `kpkg` | The front end: `install`, `info`, `index`, `keygen`, `binhost`, `delta`, `verify` and the rest |
| `kpkgadd` | Install a prebuilt package file |
| `kpkgbuild` | Build a port into a package without installing it |
| `kpkgdel` | Remove a package |
| `kpkgdepends` | Print the resolved install order, and nothing else |

`kpkgdepends` writes **one bare space-separated line to stdout and nothing else**, because the
build orchestrator parses it. Diagnostics from anything running inside the build chroot go to a
log, and every token the orchestrator reads is validated against a strict pattern, so noise fails
loudly instead of being installed as a package.

It links three of our libraries and nothing else, so it is cross-compiled early and exists on
every tree from the first bootable image onward.

## Packages

A package is a compressed tar archive plus a database entry.

| | |
|---|---|
| File name | `<name>-<version>-<release>.tar.xz` |
| Database | `/var/lib/kpkg/db/<name>` |
| Manifest | Every path the package owns, `./`-prefixed, directories with a trailing slash |
| Install hook | `.POSTINSTALL` inside the archive, when the port has one |

**A file conflict is between packages.** A path that exists but that no installed package claims
is **adopted**, not refused. That is not a loosening — it is what the self-hosting bootstrap
phase *is*: the earliest phases install a toolchain by hand, leaving files no database entry
owns, and the bootstrap then rebuilds exactly those packages with `kpkg`.

A path that another package **does** own is still a conflict. Where the userland genuinely
overlaps — the compact userland ships many names that the full GNU tools also provide — the rule
is that whoever comes last in the dependency order wins, expressed with an explicit overwrite
flag. That flag does exactly one thing besides allowing the write: **the path changes hands in
the database**, removed from the previous owner's manifest. Without that, removing the older
package would delete a file the newer one installed.

**An upgrade removes orphans.** A file present in the old version and absent from the new one is
removed rather than left on disk owned by nothing.

## Deciding what to rebuild

The build must not recompile 764 ports on every run, and must not skip one whose recipe changed.
Two hashes decide, and they are the same two the binary host uses.

**`E:` — the recipe hash.** SHA-256 over `kpkgbuild`, `build.sh`, `postinstall.sh` and every
patch, sorted by name, each contributing its name *and* its bytes. It is an exact statement of
what a package was built from.

For a port that names a `source =`, nothing else in the directory is hashed: a tarball is content
the recipe already names and the `sha256 =` beside it already covers.

**A source-less port is different, and this is the half that makes the rule true for our own
code.** A port with no `source =` builds out of its own directory — nothing names those files and
no checksum covers them. Hashing only the four recipe files would mean that editing a `.c` changes
nothing the build can see: the port reports as installed and current, and the tree keeps the
binary it already had. The symptom is never a build error; it is a shipped program behaving like
an older one. So a source-less port hashes **its whole directory**, sorted at every level, **and
all of `src/libs` with it** — because a `build.sh` names which libraries it compiles and parsing
that would be a shell parser inside the package manager.

The stated cost: editing one library rebuilds every port of ours, rather than only its consumers.
An upstream port's hash is unaffected.

**`B:` — the build-config hash.** The architecture, the C library, the target triple, the compiler
version and the compiler and linker flags. Two machines with the same `B:` produce comparable
binaries; two with different `B:` do not, whatever the recipe says.

Both hashes include their field **names**, because a hash over values alone collides the moment
two fields swap.

### The three states

Skip-if-installed compares the recorded recipe hash to the port as it stands:

| State | Result |
|---|---|
| Hashes match | Skip |
| Hashes differ | Rebuild |
| **No recorded hash, or a corrupt one** | **Skip** |

The third row is what makes this safe on a tree that predates the mechanism: absent reads as
*unknown*, never as *changed*, so no package is rebuilt merely for lacking a record. A record that
is not exactly the right shape is treated the same way — reading it as a mismatch would rebuild
that one package on every run forever with nothing saying why.

The hash is recorded **after** a successful install, never before: a record written ahead of a
build that then fails would claim a recipe is installed that is not.

`KPKG_STRICT_RECIPE=1` enables it. Every build phase environment sets it, so **the build is
strict**; an interactive `kpkg install` is not.

**The solver applies it, not the install loop.** An installed and current package is dropped
before the loop runs, so a test placed downstream would reach only packages named on the command
line and miss every *dependency* whose recipe changed.

## Reproducible packages

**A package built twice from the same tree is byte-identical.** That is a property of one
function — the archive roller inside `kpkg` — rather than of 764 recipes, which is exactly why
`kpkg` rolls the archive itself instead of letting each `build.sh` do it.

Every flag there addresses a specific source of drift:

| Flag | Without it |
|---|---|
| `--sort=name` | Directory order is filesystem order, which is not stable even between two copies of the same tree |
| `--mtime=@$SOURCE_DATE_EPOCH` | Every file carries the second it was installed |
| `--owner=0 --group=0 --numeric-owner` | The builder's user id, and its *name* as text in the header |
| `--format=gnu` | Extended headers carry access and change times, which are wall clock |
| `--use-compress-program=xz -9 -T1` | Multi-threaded compression is not deterministic, and an environment variable can silently enable it |
| `umask(022)` before the build | A file created without an explicit mode takes the builder's umask — the one source of drift that is not in the archive call |

The other half is five lines in every phase environment: a **pinned** epoch (not the current
date, and not derived from git, which the build container does not have), `TZ=UTC`, `LC_ALL=C`, a
compiler flag that rewrites source paths, and a linker flag making the build identifier a function
of the contents rather than random.

Reproducibility is not decoration. It is what makes a signed binary host meaningful, what lets a
delta reconstruct a package that still verifies against the **original** signature, and what lets
a rebuild be *compared* to what it was built from rather than merely produced.

## The binary host

```sh
kpkg keygen builder                      # once, on the machine that builds
kpkg index /repo --sign builder.key      # PACKAGES + PACKAGES.sig + a sidecar per package
cp builder.pub /etc/kdos/keys/           # on every machine that should trust it
kpkg binhost /repo zlib                  # install it, or say why it will not
```

The index is a flat file: single-character keys, one stanza per package, blank line between. It
parses in a few dozen lines of C and reads fine in a pager.

**Three equality tests decide whether a prebuilt package is usable**, and there is no "close
enough": the architecture, the build-config hash `B:`, and the recipe hash `E:`. Anything else
builds from source. The exit code says which happened:

| Exit | Meaning |
|---|---|
| 0 | The prebuilt package was used |
| 1 | No match — build it from source |
| 2 | Refused |

That is Gentoo's entire USE-flag matching problem replaced by two equality tests, and it works
because KDOS has no USE flags.

### Signing

Ed25519, through a vendored public-domain implementation — the only third-party source under
`src/`. See [The C libraries](../05-developer/c-libraries.md).

**One signature over the index covers every package transitively**, because the index carries
each package's hash. A per-package sidecar exists for the separate case of a package travelling on
a stick with no index beside it.

Rules the scheme keeps, each of which is a way signing usually rots:

- **A key id is not a key.** The id inside a signature selects which trusted key to try;
  verification always uses a key from the trusted directory. A signature that could supply its own
  key verifies nothing.
- **A bad signature is not a missing one.** Installing a package whose sidecar *fails* is refused;
  one with no sidecar is allowed, because locally built packages are the majority and are never
  signed. `KPKG_REQUIRE_SIG=1` is the stricter policy for a machine that only installs from a
  host.
- **`--insecure` says so out loud, every time.** A flag that prints nothing is a flag that gets
  left in a script.
- **The index is verified before it is believed, and a package's hash before it is unpacked.**
  Verifying after installing is verifying nothing.
- **Multi-signature from the start.** A signature file is one line per signature, so during a key
  rollover both keys sign and a client trusting either keeps working. Retrofitting that is brutal.
- **The signing key is written with restrictive permissions and exclusive creation**, and reading
  it back refuses if the mode has loosened.

**The trusted directory *is* the policy.** There is no revocation list and no online check:
adding a key is copying a file in, removing trust is deleting one. And the loader does **not**
descend into subdirectories, which is what keeps `/etc/kdos/keys` (host packages) and
`/etc/kdos/keys/packs` (application packs) genuinely separate policies.

**Ports built from source are not signed and need no signature.** A port is compiled on this
machine and its integrity is the `sha256 =` in its recipe. Signing answers "who made this binary",
and for a port the answer is "you did".

## Deltas

```sh
kpkg delta zlib-1.3-1.tar.xz zlib-1.3.1-1.tar.xz
```

Two decisions around a standard binary-diff tool are the whole of the work.

**The delta is taken over the uncompressed archives.** Two compressed files built from nearly
identical trees share almost no bytes — that is what a compressor does — so a delta between them
is the size of the whole package. Decompressing first is the entire difference between a few
kilobytes and the full package.

**A delta is never trusted, and never needs to be.** It is applied and the **result** is hashed
against the entry the signed index already carries. A tampered delta produces a package whose hash
does not match and is discarded; it cannot make a client install anything the index did not
already name. So there is no delta signature and no second trust path.

A delta appears in the index as an ordinary stanza with an extra key naming the package file it
applies to. There is no type field, because "it names what it patches" says the same thing. A
client uses one only when it still **has** that old package, which is the ordinary case for a
machine that has been updating rather than installing fresh.

## Vulnerability tracking

```sh
kdos cve
```

The question is a version comparison, not a scan: *is the version we pin older than the version
the database says fixed this issue?* The comparator is the package manager's own version
comparison, shared so that this and the upstream-version checker cannot disagree about what
"newer" means.

The data is a **vendored, pruned copy of Alpine's security database** — a committed, diffable text
file merged from a dozen Alpine branches — so the answer is offline. See
[Decisions](../01-philosophy/decisions.md) for why Alpine rather than the larger sources.

Four details, each of which changes the answer:

- **Alpine's packaging revision is stripped** before comparing. Leaving it on makes every pin look
  old.
- **"Fixed in 0" means never affected** in that branch, and falls out of the comparison for free.
- **The newest fix a pin is behind is the one reported**, because it closes every earlier one too,
  and the identifiers from all matching rows are merged.
- **A `secdb =` key in a recipe** maps a port whose name differs from Alpine's.

**A package the database does not carry is UNKNOWN, never clean.** A large fraction of the tree is
in that state and the summary says so, because a checker that counted them as fine would be
reporting a number it had not earned. The database's age is printed with every run.

`ports/update --cve` is the online cross-check, one request per port against a rate-limited
service. That cost is why the vendored table is the everyday answer.

## See also

- [Writing ports](../05-developer/writing-ports.md) — the recipe format and how to add one
- [The build system](../05-developer/build-system.md) — how phases drive `kpkg`
- [Packs and boxes](packs-and-boxes.md) — the other packaging system, and why it is separate
- [The security model](security-model.md) — the trust argument behind the keyrings
- [The kdos command](../04-programs/kdos-command.md) — `kdos cve` and `kdos update`
