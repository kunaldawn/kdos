# Firewall

The KDOS [firewall]{.idx} is one nftables ruleset, loaded at boot before the network starts. It
drops what arrives unasked, apart from the few things a workstation has to receive: ICMP, mDNS,
DHCPv6, NetBIOS name replies, and a [hotspot]{.idx}'s DHCP and DNS requests. It forwards nothing
but the traffic of a hotspot, rootful containers and libvirt guests, and lets everything out. This
page covers the files that make up the ruleset, opening a named service from the desktop or the
command line, adding rules of your own, checking what is loaded, and what to do when the ruleset
does not load.

## Installation

[nftables](#port-nftables) provides [`nft`]{.idx}, and is installed by `41_system` on every image. The
ruleset and the script that loads it are part of the base filesystem: `fs/etc/nftables.conf`,
`fs/etc/nftables.d/` and `fs/etc/init.d/25_nftables.sh`.

The service is `nftables`. It is enabled on the live medium and on an installed system alike, and
the installer offers no choice about it. It is not supervised: `nft` hands the ruleset to the
kernel and exits. It is also the one service [`/etc/init.d/rcK`]{.idx} does not stop at shutdown.

Opening a named service goes through [kdos-powerd](#port-kdos-powerd), which owns the list of
names, and its two clients: `kdos-power firewall` on the command line and `kdos-firewall`, the
Firewall tool of [kdos-shell](#port-kdos-shell). [iptables](#port-iptables) is installed too,
built over the nftables kernel interface; the firewall does not use it.

## Configuration

### /etc/nftables.conf

The policy, from `fs/etc/nftables.conf`. It deletes and rebuilds its own table, `inet filter`, and
nothing else, then includes every drop-in last:

<!-- kfs:include fs/etc/nftables.conf -->

What each rule is for is in [The firewall](../../kdos/02-user-guide/administration.md#the-firewall)
and [The firewall](../../kdos/03-architecture/security-model.md#the-firewall).

### /etc/nftables.d/

Every `*.nft` file here is included at the end of `nftables.conf`, and must add to
`table inet filter` without declaring a table or a chain hook of its own. The directory's
`README`, from `fs/etc/nftables.d/README`:

<!-- kfs:include fs/etc/nftables.d/README -->

| File | Written by | Opens |
|:--|:--|:--|
| `40-podman.nft` | The base filesystem, `fs/etc/nftables.d/` | Forwarding and DNS for rootful podman's `podman*` bridges |
| `40-libvirt.nft` | The [libvirt](#port-libvirt) package | Forwarding, DHCP and DNS for libvirt's `virbr*` bridges |
| `50-kdos-services.nft` | The base filesystem, empty; rewritten whole by [`kdos-powerd`]{.idx} | The named services that are on |

The shipped `50-kdos-services.nft`:

<!-- kfs:include fs/etc/nftables.d/50-kdos-services.nft -->

### /etc/init.d/25_nftables.sh

The `start` branch checks the whole ruleset with `nft -c` and loads it only when the check passes:

<!-- kfs:include fs/etc/init.d/25_nftables.sh lines=65-94 pin=51e285d29eee -->

When the check fails and no `inet filter` table is loaded yet, as at boot, `load_fallback` loads
`nftables.conf` without its `include` line, and when that fails too, a minimal drop policy written
to `/run/nftables-fallback.nft`. Either way the script reports `[FAIL]`. `stop` deletes
`inet filter` and nothing else.

## Usage

### Seeing what is loaded

1. Ask the service:

   ```bash
   sudo service status nftables
   ```

   It answers `nftables: ruleset loaded` or `nftables: no ruleset loaded`. Run it with `sudo`:
   reading the ruleset needs root, and as an ordinary user the script's `nft list ruleset` fails and
   it reports `no ruleset loaded` whatever is loaded.

2. List the rules the kernel holds:

   ```bash
   sudo nft list table inet filter    # this firewall
   sudo nft list ruleset              # with the tables other programs add
   ```

3. List the named services and whether each is open (members of `wheel`):

   ```bash
   kdos-power firewall list
   ```

   `nft list ruleset` on a fresh boot shows `table inet filter` and libvirt's own
   `ip libvirt_network` and `ip6 libvirt_network`; podman and a NetworkManager hotspot add theirs
   while they run.

### Opening a named service

The names, and the ports each one opens, are compiled into `kdos-powerd`; the table is in
[firewall](../../kdos/04-programs/daemons.md#firewall). Turning one on or off needs `wheel`.

1. With the desktop: open Settings, go to **System**, and choose **Firewall…** under *Security and
   updates*. Select the service's row and press Enter or Space, or click **Open** (**Close** for
   one that is open). The *STATE* column shows `open` or `closed`.

2. From a shell:

   ```bash
   kdos-power firewall https on
   kdos-power firewall https off
   ```

   A successful change prints `ok https on`. The daemon rewrites `50-kdos-services.nft`, checks the
   whole ruleset and reloads it. The change is in force at once and at every later boot: the
   rewritten file holds the rule, `tcp dport 443 accept`, and so does
   `sudo nft list chain inet filter input`.

3. Start the daemon that answers on the port, if it is not running: opening `ipp`, for example,
   does not start CUPS. `ssh` is the exception: turning it on also starts `sshd` and enables it at
   boot; see [SSH server](ssh-server.md).

### Adding a rule of your own

For a port no name covers.

1. Create a file of your own in [`/etc/nftables.d`]{.idx}, for example [`/etc/nftables.d/60-local.nft`]{.idx},
   which re-opens the `input` chain without a `type … hook` line:

   ```text
   table inet filter {
       chain input {
           tcp dport 3000 accept
       }
   }
   ```

   To open a port to one network only, match the source as well:
   `ip saddr 192.168.1.0/24 tcp dport 3000 accept`.

2. Check the whole ruleset without applying it:

   ```bash
   sudo nft -c -f /etc/nftables.conf
   ```

   No output means it would load. An error names the file and the line, as in
   `/etc/nftables.d/60-local.nft:3:29-29: Error: syntax error, unexpected newline`.

3. Apply it:

   ```bash
   sudo service start nftables
   ```

   `start` reloads: the file deletes and rebuilds `inet filter` in one transaction, so a running
   container's address translation and a hotspot's tables survive.

4. Do not write into `50-kdos-services.nft`. `kdos-powerd` rewrites it whole at the next toggle,
   and a rule it does not recognise is dropped.

### Turning the firewall off

1. For this boot only:

   ```bash
   sudo service stop nftables
   ```

   It prints `[KDOS] Removing nftables ruleset...` and deletes `inet filter` alone; libvirt's and
   podman's tables stay. The machine then accepts every connection until the next `start` or the
   next boot.

2. At every boot, with `sudo service disable nftables`, which writes
   [`/etc/service.disabled/nftables`]{.idx} and prints `Service 'nftables' disabled.`
   `sudo service enable nftables` removes it; the marker is described in
   [Choosing what starts at boot](services.md#choosing-what-starts-at-boot).

On the live medium the `disable` marker is lost at power-off unless a
[persistence store](live-persistence.md) exists.

## Tips

- `sudo nft monitor` prints every change to the ruleset as it happens, which shows whether a
  toggle reached the kernel.
- To log what the input chain drops while you debug, add `log prefix "nft-drop: " drop` to the
  `input` chain in a file that sorts after every other drop-in, such as `99-log.nft`; the drop-ins
  are read in name order, so a rule in a later file would never be reached. `sudo dmesg` shows each
  packet (the kernel log is root's). Remove the file afterwards.
- `iptables` on KDOS is the nftables-backed `xtables-nft-multi`, and its rules go into tables of
  their own, which `nftables.conf` neither loads nor removes and nothing restores at boot. An
  `accept` there does not let in a packet this ruleset drops: a drop in any base chain on a hook is
  final. Write rules in a drop-in instead.
- `kdos why /etc/nftables.conf` reports that no package owns the file: it comes from `fs/`.

## Troubleshooting

**The boot shows `[FAIL] nftables: /etc/nftables.conf does not load`.**
Read the script's boot log, which holds `nft`'s own error lines:

```bash
cat /run/kdos-init.nftables.log
```

The rest of the line says what was loaded instead: `loaded it without /etc/nftables.d` (a drop-in
is wrong) or `loaded the minimal drop policy` (the main file is wrong). Fix the file the error
names, check with `sudo nft -c -f /etc/nftables.conf`, and run `sudo service start nftables`.
`no ruleset could be loaded — input is NOT filtered` means even the fallback failed; check that the
kernel has nftables with `sudo nft list tables`.

Run by hand on a machine whose `inet filter` table is already loaded, the same failure loads
nothing and ends `keeping the current ruleset`.

**`nft -c` says `Chain "input" already exists in table inet 'filter' with different declaration`.**
The drop-in repeats the chain's `type filter hook …` line with another priority. Remove that line;
naming the table and the chain alone re-opens the existing one. A `type … hook` line that matches
the main file's exactly is accepted and adds to the same chain, so the check does not catch it,
and a `policy` on that line replaces the chain's own; leave both out of a drop-in.

**`kdos-power firewall <name> on` prints `err the ruleset would not load; nothing changed`.**
A hand-written drop-in is broken, so the whole ruleset fails its check. The kernel keeps the rules
it had, and the daemon puts the previous `50-kdos-services.nft` back, so `firewall list` and the
next boot keep the previous state. Run `sudo nft -c -f /etc/nftables.conf`, fix the file it names,
and repeat the toggle. `err the ruleset did not apply` is the same refusal from the kernel after the
check passed, and leaves the same previous state.

**`kdos-power firewall <name> on` prints `err cannot read the current rules`.**
`50-kdos-services.nft` exists but the daemon cannot read it, so it could not put the file back if
the new ruleset were refused, and it changes nothing. Check the file with
`ls -l /etc/nftables.d/50-kdos-services.nft`; it belongs to root and is readable by its owner.
If it is damaged, delete it and repeat the toggle: the daemon writes the file whole, and with no
file every service starts from off, so turn the others back on afterwards.

**`kdos-power firewall` prints `err no service called that`.**
The name is not in the daemon's table. Run `kdos-power firewall list` for the names, or write a
drop-in.

**`kdos-power firewall` prints ``kdos-power: no kdos-powerd — `service start powerd` ``, or `err not permitted`.**
Start the daemon with `sudo service start powerd`, or run the command as a member of `wheel`.

**A hotspot's clients join and get no address.**
The input rules for `10.42.0.0/16` are missing or a drop-in drops that traffic first. Check
`sudo nft list chain inet filter input` for the `ip saddr 10.42.0.0/16` lines.

**A rootful container reaches nothing, or a libvirt guest gets no address.**
Check that `40-podman.nft`, or `40-libvirt.nft`, is in `/etc/nftables.d`, and that
`sudo nft list chain inet filter forward` shows its rules.

**A rule added by hand vanished.**
It was in `50-kdos-services.nft`, which `kdos-powerd` rewrites whole. Move it to a file of your own.

## Known gaps

- [Security](../../kdos/06-reference/known-gaps.md#security): the summary of what the system,
  the firewall included, does not protect, with the live medium's known passwords
- [What is not protected](../../kdos/03-architecture/security-model.md#what-is-not-protected): the
  full statement

## See also

- [The firewall](../../kdos/02-user-guide/administration.md#the-firewall),
  [Opening a service](../../kdos/02-user-guide/administration.md#opening-a-service) and
  [Adding your own rules](../../kdos/02-user-guide/administration.md#adding-your-own-rules): the
  user guide
- [The firewall](../../kdos/03-architecture/security-model.md#the-firewall): the policy and why
  the load is checked
- [firewall](../../kdos/04-programs/daemons.md#firewall): the `kdos-powerd` verb and its table
- [/etc/nftables.conf](../../kdos/06-reference/configuration.md#etcnftablesconf): the reference
- [SSH server](ssh-server.md), [Services](services.md), [NetworkManager](networkmanager.md),
  [Boxes](boxes.md), [Virtual machines](virtual-machines.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 384.* Previous: [383. Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md) · [Contents](../README.md) · Next: [385. SSH server](ssh-server.md)
