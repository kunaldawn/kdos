# The security model

Who is allowed to do what on a KDOS machine, which mechanisms enforce it, and — as prominently —
what is **not** protected. A security model that only lists its defences is a security model that
will be relied on for things it does not do.

## What this model is for

KDOS is a **single-user workstation**. One human account ships, that account is in `wheel`, and
`wheel` is trusted. The threats this design actually addresses are:

- Software from the outer ring — a browser, an office suite, an application from someone else's
  distribution — doing things a desktop application has no business doing.
- A tampered or substituted **artefact**: a package, an index or an application image that is not
  what it claims to be.
- Privilege escalation through the small number of programs that genuinely need privilege.
- A local user in `wheel` making a catastrophic mistake through an interface that should not have
  allowed it to be expressed.

The threats it does **not** address are in [What is not protected](#what-is-not-protected) at the
end. Read that section before relying on any of this.

## setuid binaries

The shipped system carries **seventeen** setuid-root binaries. **Exactly two are ours.**

| Binary | Origin | For |
|---|---|---|
| `kdos-checkpass` | **KDOS** | Checking the caller's own password against the shadow file |
| `kdos-resctl` | **KDOS** | Signalling and renicing a process from the resource monitor |
| `sudo`, `su` | sudo, util-linux | Running as another user |
| `passwd`, `chage`, `expiry`, `gpasswd`, `chfn`, `chsh`, `newgrp` | shadow | Account management |
| `pkexec`, `polkit-agent-helper-1` | polkit | Authorised privileged actions |
| `ssh-keysign` | OpenSSH | Host-based authentication |
| `dbus-daemon-launch-helper` | dbus | System bus activation |
| **`newuidmap`, `newgidmap`** | shadow | **Rootless containers** |

**The last two are why every application on the machine works.** The container engine runs them to
write a process's user-namespace map, the kernel allows that only from a process already holding
the relevant capability, and the engine checks the binary first and refuses outright if it is not
setuid — then exits with nothing else printed. Lose those bits and every graphical application
stops starting, with nothing saying why.

**Mode bits rather than file capabilities**, deliberately: mode bits survive all three hops the
system makes them take — the compressed system image, the installer's copy, and the pack image
format — with no extended attribute anywhere in the chain.

`kdos doctor` checks all four of the critical ones, because losing a setuid bit is the worst
*silent* failure in the system. An archive copy without the right flag is all it takes.

## kdos-checkpass

The program that locks you out if it is wrong, so it is worth stating in full what it does not
do.

- **It takes no arguments.** Not even a user name. The account checked is the **caller's real user
  id**, so there is nothing to aim at root and nothing an attacker can vary.
- **The password arrives on stdin**, never in the argument vector, because a process's command
  line is world-readable for its whole lifetime.
- **Privilege is dropped as soon as the hash is read.**
- **The comparison is constant time.**
- **A locked, disabled or empty hash always fails.**
- **Three exit codes** — correct, wrong, error — and the caller must distinguish them. Reporting
  "wrong password" for a machine with a broken shadow file sends the user looking in entirely the
  wrong place.

Without its setuid bit it cannot read the shadow file, so it **refuses every password** and locks
the user out of their own session.

**The console greeter uses it too, and holds no hash of its own.** `kdos-con-login` runs as root
from `/etc/inittab`; on submit it forks, and the child does `initgroups`, `setgid`, `setuid` — in
that order, because `setuid` first would drop the privilege the other two need — and then executes
`kdos-checkpass`. After the drop the caller **is** the candidate, so the "no arguments, own uid"
rule above covers the greeter without a second mechanism, and no crypt implementation is linked
into a program that draws on a screen.

## The console session's two sockets

`$XDG_RUNTIME_DIR/kdos/<name>.sock` admits **surfaces** — programs that place windows.
`<name>.view` admits **views** — a display, which holds no window state at all.

**Which socket a client reached decides what it is allowed to be.** The kind in a client's
handshake is a claim and is overridden by the listener's. That is the entire reason there are two:
only the view socket is ever forwarded, and forwarding one that admitted surfaces would hand the
far end the right to place windows in a session rather than the right to display it.

Both are gated by `SO_PEERCRED` against the session's owner, inside a directory the session creates
`0700`. A directory that already exists with the wrong mode or owner is **a refusal to start, not a
`chmod`**: if it is not ours, taking it over puts the socket in a path another account chose, after
which the credential check is guarding the wrong door.

**There is no TCP listener.** A remote desktop is a forwarded unix socket and inherits ssh's
authentication, which is why it needs none of its own — and that argument holds only while there is
no other way in, so the self-test asserts the absence by grepping those sources for `AF_INET`.
`remote = no` is enforced where the tunnel is built rather than where a connection arrives: a
forwarded socket's peer is the local ssh process running as the same user and cannot be told from a
local view by credentials, so `kdos con forward` refuses to build the tunnel at all.

## kdos-resctl

The second setuid binary, and its whole security argument is that **there is nothing to aim**:
three verbs, no paths, no options.

```
kdos-resctl dmi
kdos-resctl signal <pid> <TERM|KILL|STOP|CONT>
kdos-resctl renice <pid> <-20..19>
```

- The hardware-table path is compiled in. **Nothing in the argument vector is ever opened.**
- The caller must be in `wheel` **by real user id**.
- A process handle is taken **before** the signal is sent, so a signal cannot land on a recycled
  process id.
- Process 1 is refused.
- Privilege is dropped **before** the hardware table is parsed, so the parser never runs as root.
- It is **never on the sampling path**. A setuid fork once a second would be an attack surface
  with a schedule.

## Root daemons

Six daemons run as root and answer a socket. They share one authorisation design:

**The gate is the peer's credentials, not the socket's mode.** Every socket is mode 0666 and
anyone may connect; the daemon reads the connecting process's real user id from the kernel and
answers `err not permitted` to anyone who is not root or in `wheel`. A mode that *looked* like the
authorisation is a mode somebody eventually loosens, and there is nothing in the message a client
can forge.

**The client never names a path.** Every verb takes an identifier out of a list the daemon itself
published a moment earlier. There is nothing to aim: the design where a daemon takes a device and
a mount point ends at mounting a stick over `/etc` from any shell in `wheel`.

The one deliberate exception is installing a pack, which names a **filename in a staging directory
the daemon owns** — the single place an unprivileged write is allowed, and the daemon publishes
that path rather than making clients derive it.

Per-daemon refusals:

| Daemon | Refuses |
|---|---|
| `kdos-mountd` | Internal disks; filesystems the kernel cannot mount; anything in `fstab`; **every partition of the disk the system booted from**; a verb carrying a token nobody named; an index that is not a number; a filesystem outside the four it will write; a `format` not confirmed with the device's own kernel name; a node whose device differs from the one the scan recorded |
| `kdos-packd` | Paths as arguments; a pack whose hash or signature fails; removing a pack that is in use |
| `kdos-oomd` | Any argument at all — killing is its own decision or it does not happen |
| `kdos-energyd` | Republishing the raw counter; a client-chosen sampling interval |
| `kdos-powerd` | Anything but four fixed words |

**`kdos-energyd` deserves its own note.** The CPU energy counter has been root-only since a
side-channel attack showed that fine-grained unprivileged reads can recover cryptographic keys.
What leaves this daemon is a per-application percentage over minutes; the raw counter and the
interval are never republished, and **the interval is fixed by the daemon rather than requested by
a client**, so it cannot be driven toward being one. There is no write path into the power
interface at all.

## Sandboxed clients

A client from a box is tagged by `kdos-boxsock` with a security context naming its box, and the
compositor's filter gives such clients a **fixed allowlist**:

| Allowed | Denied |
|---|---|
| Surfaces and rendering | Screen capture, both generations |
| The seat | Buffer export |
| Buffer sharing | Data-control (clipboard manipulation) |
| **Text input** | **Input method and virtual keyboard** |
| The primary selection | Output management |

Two entries in that table are the interesting ones.

**Text input is deliberately allowed** — that is the *application* half of the input-method
protocol, and denying it would deny input methods to exactly the applications that need one most.

**The input-method and virtual-keyboard interfaces are denied**, because a client that can be an
input method receives every keystroke on the seat. That is a keylogger by design, and it is not
something an application from someone else's distribution gets to be.

**This is what makes the portal the sanctioned route rather than a convenience.** A boxed screen
recorder cannot bind the capture interface at all, so it must ask the portal, which runs on the
host and asks you which output to share. The denial is what gives the question meaning.

## Containers

Boxes are **rootless**. The container engine runs as your user, mapping your identity into the
container, with the setuid mapping helpers above doing the one privileged step.

**Ownership inside a box grants nothing**, because packs are mounted `nosuid`. Running as a
mapped non-root user rather than as mapped-root is chosen because applications refuse to run as
root, not because it is a boundary — a process in your box is a process running as you.

**A box is not a security boundary against you.** It shares your home directory in full. What it
is is a boundary against *the desktop's* interfaces — the compositor globals above — and a
packaging mechanism.

Box profiles are honest about this. Every key maps onto something actually enforced, and the
profile **says out loud what it cannot enforce**: there is no engine flag that grants a box a
speaker and denies it a camera, so the profile does not pretend to have one. A memory budget is
enforced by `kdos-oomd` rather than by the engine, because rootless containers on a machine with
no cgroup delegation accept a memory limit and ignore it.

## Mount options

| Mounted | Options |
|---|---|
| Application packs | `ro,nosuid,nodev` |
| Data packs | `ro,nosuid,nodev,`**`noexec`** |
| Removable media | `nosuid,nodev`, and `noexec` by default |
| `/tmp`, `/run` | `nosuid,nodev` |

`exec = yes` in the removable-media configuration is how somebody says they meant it. A setuid
root binary on a stick from another machine is a local root hole that predates every other
consideration on this page.

## Signing and trust

Two keyrings, and their separation is **structural rather than a convention**:

| Directory | Attests | Used for |
|---|---|---|
| `/etc/kdos/keys` | Who built a host package | The binary host index and package sidecars |
| `/etc/kdos/keys/packs` | Which bake an application image came from | The pack index |

The keyring loader reads `*.pub` in a directory and **does not descend**, so the two are genuinely
different policies. A pack-signing key placed in the host directory would silently become a
trusted publisher of *host packages* too — a widening nobody asked for.

**The directory is the policy.** There is no revocation list and no online check: adding a key is
copying a file in, removing trust is deleting one. A key id is not a credential — it selects which
trusted key to try, and verification always uses a key from the directory.

The rest of the signing design is in [Packaging](packaging.md).

## Untrusted image bytes

**Anything that can write to a terminal can reach an image decoder.** A shell script, a program
inside a box, `cat` on a file somebody sent you — three escape sequences carry a picture, and
behind them are four large C libraries with long memory-safety histories.

That is why `libkimg` is a library with **one entry point** rather than four calls scattered
through a terminal: one place to audit, one place the budget is enforced, one place a fifth format
would be added.

- **The budget is enforced before any allocation**, from the size the format itself declares. A
  length field is an allocation request from an untrusted peer: a decompression bomb is four lines
  of sixel, and a PNG saying 65535x65535 is eight bytes on the wire and sixteen gigabytes in memory.
  Refusing after decoding is not refusing.
- **A declared type that disagrees with the bytes is a refusal, not a re-sniff.** A peer that says
  PNG and sends sixel is not making a mistake worth accommodating.
- **The payload is capped, and a payload over the cap is dropped entirely rather than truncated.**
  Half an image is not a smaller image; it is a malformed file, and handing one to a decoder is
  handing it exactly what an attacker would have sent on purpose.
- **Both decoders are silenced.** libpng and libjpeg write to stderr by default, which is untrusted
  bytes deciding what appears on a terminal.
- **Every failure returns the same nothing.** The caller cannot tell a truncated file from an
  unsupported format from a budget refusal, on purpose: there is nothing useful to do differently,
  and a reason string reaching a log is a reason string an attacker chose.
- **The corpus is fuzzed**, every fixture truncated at every length and with every byte flipped,
  under the address and undefined-behaviour sanitizers.

**A build without the decoders turns the three protocols off in the parser**, rather than parsing
them and dropping the result. Parsing bytes nobody can use is a buffer somebody can fill.

## The one descriptor, and the two protocols that carry none

**The surface protocol and the view protocol carry no file descriptors, ever.** That is not a
minimalism: it is what lets the view socket be forwarded over `ssh`, so a desktop reached from
another machine is the same desktop rather than a second implementation. A sprite travels as its
pixels and is cached by key; nothing on either socket is a handle to anything.

**One descriptor exists, on a third channel that is neither.** An embedded application's compositor
is a child of the session, and its frames come back over a `socketpair` created *before* the fork
and inherited — never a path anything can connect to, never a name in a directory, never reachable
by a process that is not that child. The shared mapping it passes is what a frame is; passing it any
other way would mean copying every frame through the session, which is the process whose whole
purpose is to hold no pixels.

So the rule is stated as a boundary rather than as a habit: **a descriptor may cross a channel with
exactly one peer that this process forked.** Anything a stranger can connect to carries bytes.

**`testing/selftest.sh` holds the boundary.** It greps `libkcon`, `kdos-con` and `kdos-view` for
`SCM_RIGHTS` and allows exactly one file — the session's end of that private pair. A descriptor
added to either published protocol is a broken build rather than a property somebody has to
remember, which is the only form a rule like this survives in.

## What is not protected

Stated plainly, because a reader who assumes otherwise is worse off than one who knows.

- **There is no mandatory access control.** No SELinux, no AppArmor. A process running as you can
  do anything you can do.
- **There is no verified boot and no signed kernel.** The boot chain is not measured or attested.
  An attacker with physical access and a moment alone with the machine owns it.
- **Disk encryption protects data at rest only.** It is a passphrase in the initramfs, not a
  TPM-sealed key, and it does nothing once the machine is running.
- **A box is not a jail.** It shares your home directory. A malicious application in a box can
  read and destroy your files exactly as a native one could. The sandbox constrains what it can do
  to the *desktop*, not to your data.
- **`wheel` is effectively root.** `sudo` and polkit both grant it, and every root daemon answers
  it. There is no separation between "can change the theme" and "can reformat the disk".
- **A base naming a container registry fetches unsigned content** from somebody else's server.
  This is an online operation, the strict-signature setting does not cover it, and the tool
  announces it before doing anything rather than pretending otherwise.
- **An unsigned pack mounts.** Only a *failed* signature is refused, so a pack that is signed and
  uncheckable is treated more harshly than the same pack with no signature block at all.
- **Ports built from source are not signed**, and need not be: a port's integrity is the checksum
  in its recipe. But that means the recipe — and therefore this repository — is the trust root for
  everything on the host.
- **There is no automatic update path and no security-advisory service.** `kdos cve` tells you
  what is behind a known fix; acting on it is a rebuild you perform.
- **`kdos-oomd` has never fired in anger.** Its victim selection is exercised against recorded
  system state, but a genuine memory-pressure stall is the test that matters and has not been run.

## See also

- [Packaging](packaging.md) — signing, the index, and the three equality tests
- [Packs and boxes](packs-and-boxes.md) — verification at mount time, and mount options
- [The daemons](../04-programs/daemons.md) — each daemon's verbs and refusals
- [The session](session.md) — the sandbox allowlist and the portal route
- [Known gaps](../06-reference/known-gaps.md) — everything else that does not exist
