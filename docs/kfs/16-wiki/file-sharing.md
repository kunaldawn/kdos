# File sharing

This page covers reaching files on other machines and serving files to them: mounting a Windows
([SMB]{.idx term="Samba"}) share as an ordinary user, mounting and exporting [NFS]{.idx}, running a Samba server, keeping
folders in step with Syncthing, and sending a single file with `kdos share`. Finding the machines
on the network by name is on [Avahi and mDNS](avahi-and-mdns.md); the [firewall]{.idx} names each server
needs are explained in the KDOS book's
[Opening a service](../../kdos/02-user-guide/administration.md#opening-a-service).

## Installation

Every port on this page is in an image phase's list, so the image carries all of them; none
needs installing.

| Task | Ports | What starts it |
|:--|:--|:--|
| Opening an SMB share as a user | [kio-extras](#port-kio-extras), its `smb:/` worker, in Dolphin and every KDE program | the program, on use |
| Mounting an SMB share as root | [cifs-utils](#port-cifs-utils) | `mount -t cifs`, by hand |
| Mounting with a Kerberos ticket | [krb5](#port-krb5), [cifs-utils](#port-cifs-utils) | the kernel, through `request-key`, on the mount |
| Serving SMB | [samba](#port-samba), with [kdenetwork-filesharing](#port-kdenetwork-filesharing) for Dolphin's **Share** tab | `83_samba`, at boot once `sambashare` has a member or something is shared; the first login of an account just added to `sambashare` starts it through [kded-kdos](#port-kded-kdos) and [kdos-powerd](#port-kdos-powerd) |
| Mounting and serving NFSv4 | [nfs-utils](#port-nfs-utils) | `72_nfsd`, once [`/etc/exports`]{.idx} names a share; `63_gssd`, once [`/etc/krb5.keytab`]{.idx} exists |
| Syncing folders | [syncthing](#port-syncthing) | **File Sync** in the application menu, by hand |
| Sending one file | [croc](#port-croc), [kdos-tools](#port-kdos-tools) (`kdos share`) | by hand |
| Shares in GTK file managers | [gvfs](#port-gvfs) | the session bus, on first use |

`72_nfsd`, `63_gssd` and `83_samba` are written by their ports' `build.sh` and exist only while
the port is installed. None of the three servers is in the installer's list of service choices,
so they behave the same on the live medium and on an installed system. Each skips itself at boot,
with a `[SKIP]` line, while it has nothing to serve: `83_samba` until some account can share, the
other two until their configuration file exists. The lines are in `/run/kdos-init.nfsd.log`
and `/run/kdos-init.gssd.log`: `[SKIP] nfsd: no share in /etc/exports` and
`[SKIP] gssd: no /etc/krb5.keytab`. `service status nfsd` names the daemon it
watches, `nfsv4.exportd`. What differs is where the configuration lives. On the live medium
`/etc` is in memory and is lost at power-off unless a persistence store exists
([`kdos persist`](../../kdos/04-programs/kdos-command.md#kdos-persist)); on an installed system it
is on the root filesystem.

## Configuration

### /etc/request-key.d/cifs.spnego.conf

A Kerberos mount needs the kernel to ask [`cifs.upcall`]{.idx} for a ticket. The krb5 port installs the
rule that joins them:

<!-- kfs:include ports/core/auth/krb5/build.sh lines=60-62 pin=866a2c35ec7c -->

### /etc/samba/smb.conf

The image ships a standalone server whose shares are the users' own usershares, made from
Dolphin's **Share** tab, from `fs/etc/samba/smb.conf`:

<!-- kfs:include fs/etc/samba/smb.conf -->

The samba port's service script starts [`smbd`]{.idx} alone: no `nmbd` and no `winbindd`. With a
non-empty `smb.conf` it makes the usershare directory at every boot, root's, group `sambashare`,
mode 1770, which a package cannot carry, and then starts `smbd` only if some account can share:
the `sambashare` group has a member in `/etc/group`, or something is already there to serve, a
regular file in that directory, not a symbolic link, which `smbd` would refuse, whose name does
not begin with `:` (`net usershare` writes each share through a `:tmp` file it renames), or a
section of `smb.conf` other than `[global]`. Otherwise it prints
`[SKIP] samba: no account shares folders`, and nothing listens on the SMB ports:

<!-- kfs:include ports/core/servers/samba/build.sh lines=64-107 pin=175ae4664f83 -->

`smbd` has to be running before the first share can be made. `net usershare add` turns every
name in a share's ACL, Dolphin's `Everyone` among them, into a SID by asking the local `smbd`, and
with `smbd` down it fails with "cannot convert name … to a SID" and writes no share. An account
the Share tab has just added to `sambashare` is a member from its next login, which can come
before the next boot, so [kded-kdos](#port-kded-kdos), loading in a session that can write into
the usershare directory, calls `StartFileSharing` on [kdos-powerd](#port-kdos-powerd)'s
`io.kdos.Power1` once. The daemon asks polkit (`io.kdos.filesharing.start`, granted to the person
at the machine with no prompt) and runs `/etc/init.d/83_samba.sh start`, which makes the same test
again and starts nothing when `smbd` already runs; it refuses while
`/etc/service.disabled/samba` exists. Leaving the group or removing the last share leaves `smbd`
running, serving nothing, until the next boot, which then does not start it.

The configure options set where Samba keeps its files: the configuration under [`/etc/samba`]{.idx}, the
lock files under `/var/lib/samba/lock`, the password database under `/var/lib/samba/private` and
the logs under [`/var/log/samba`]{.idx}:

<!-- kfs:include ports/core/servers/samba/build.sh lines=16-25 pin=0e692c40a402 -->

The lock directory is not Samba's default `/var/lock/samba`, which is in the world-writable
`/run/lock`: any Samba client run before `smbd`, `net usershare` among them, makes it there as its
user, and `smbd` then refuses the `msg.lock` directory inside, which is not root's, and exits at
every start. `/var/lib/samba/lock` is the package's, root's and mode 0755, so a client makes
nothing in it and still works.

`smbd` creates neither `/var/log/samba` nor `/var/cache/samba`. Both exist on the live medium and
on an installed system, because the package claims them and the image carries every directory a
package claims under `/var/log` and `/var/cache`, empty ([110_iso](../15-image/110-iso.md)). The
service script also creates both before each start, so emptying `/var/log` does not cost the
server its log. Without them, `testparm` reports `ERROR: cache directory /var/cache/samba does
not exist`, and the server's log lines go only to the supervisor's `/run/kdos-svc.samba.log`,
among them *Unable to open new log file '/var/log/samba/log.smbd'*.

### /etc/exports and the NFS scripts

nfs-utils installs two service scripts. `72_nfsd` is the NFSv4 server; it skips itself until
`/etc/exports` has a line that is not blank or a comment, and starts the kernel server with
NFSv3 turned off (`-N 3 -V 4`):

<!-- kfs:include ports/core/filesystem/nfs-utils/build.sh lines=42-85 pin=cf2fd59498b6 -->

`63_gssd` is the Kerberos half of an NFS client, for a `sec=krb5` mount, and skips itself until
`/etc/krb5.keytab` exists and is not empty:

<!-- kfs:include ports/core/filesystem/nfs-utils/build.sh lines=87-118 pin=636d88dec6c6 -->

### Syncthing's first configuration

[`kdos-syncthing`]{.idx}, the command behind **File Sync**, gives a user who has no Syncthing configuration
this one, which keeps discovery on the local network and turns off the global announce server,
relays, NAT traversal and usage reporting:

<!-- kfs:include ports/core/transfer/syncthing/build.sh lines=25-39 pin=d26f6670d135 -->

The wrapper looks for an existing configuration first, and writes the file above only where there
is none:

<!-- kfs:include ports/core/transfer/syncthing/build.sh lines=42-64 pin=955202c41d00 -->

### croc

`kdos share` hands the transfer to `croc`, and `/usr/bin/croc` is a wrapper that keeps every
transfer on the local network. `croc-relay` is the same program with upstream's default relay:

<!-- kfs:include ports/core/transfer/croc/build.sh lines=23-28 pin=316ed867bd46 -->

## Usage

### Opening an SMB share from the desktop

1. In Dolphin, open **Network** in the Places panel, then **Shared Folders (SMB)**, which lists the
   file servers that answer mDNS, NetBIOS or WS-Discovery; or type the address in the location bar:
   `smb://fileserver/projects`.
2. Give your account on the server and its password when asked; **Remember password** keeps it
   in KWallet.
3. The share opens as a folder. **Add to Places** keeps it in the Places panel. Every KDE program
   opens and saves files there directly; a program that is not KDE's is given a local copy.

GTK applications reach the same shares through gvfs, as `smb://server/share`.

### Mounting an SMB share from a terminal

As root, `mount.cifs` mounts a share into the filesystem, for programs that need a real path:

```bash
sudo mkdir -p /mnt/share
sudo mount -t cifs //fileserver/projects /mnt/share -o username=you,uid=$(id -u),gid=$(id -g)
```

With a Kerberos ticket, `kinit you@EXAMPLE.ORG` first and mount with `-o sec=krb5,cruid=$(id -u)`.
Mount on a directory of its own rather than on `/mnt`: on the live medium `/mnt` holds the boot
medium at `/mnt/iso` and the persistence store at `/mnt/persist`, and a mount over `/mnt` hides
both until it is undone.

### Mounting an NFS export

1. Mount it as root, naming version 4, on a directory of its own:

   ```bash
   sudo mkdir -p /mnt/share
   sudo mount -t nfs4 fileserver:/srv/share /mnt/share
   ```

   `mount.nfs4` returns once the mount is made, and `sudo umount /mnt/share` undoes it. `/etc/mtab` is a
   link to the kernel's own list, `/proc/self/mounts`, so the helper keeps no table of its own.

2. An NFSv3 server needs `-o nolock` on the client, which keeps locks on this machine (not tried
   against an NFSv3 server; this machine's own server speaks NFSv4 only):

   ```bash
   sudo mount -t nfs -o vers=3,nolock fileserver:/srv/share /mnt/share
   ```

3. To let an ordinary account mount the export, list it in `/etc/fstab` with the `user` option
   and `noauto`, so the boot does not try it:

   ```
   fileserver:/srv/share  /mnt/share  nfs4  user,noauto  0 0
   ```

   The account then runs `mount /mnt/share` without `sudo`. Of the `fstab` lines an ordinary
   account might mount, only an NFS one works this way: util-linux's `mount` is not setuid, but
   it hands the line to `mount.nfs`, which is setuid root and checks the `user` option itself (see
   [setuid binaries](../../kdos/03-architecture/security-model.md#setuid-binaries) and
   [Adding a disk to fstab](filesystems-and-fstab.md#adding-a-disk-to-fstab)). Unmounting still
   takes `sudo umount /mnt/share`: an ordinary account may unmount only a mount recorded as its
   own, and nothing records it, because `/etc/mtab` is the kernel's list and keeps no owner.
   Neither half has been tried on a booted image.

### Serving a folder over SMB

1. In Dolphin, right-click the folder → **Properties** → **Share**.
2. Tick **Share with SMB**. The first time, the tab says you are not in `sambashare`: **Make Me a
   Group Member** asks for an administrator's password. Log out and in again; the tab says to
   restart the system, but a new login is enough, and that login starts `smbd` with no prompt.
3. Back on the tab, **Set Samba password** asks for an administrator's password a second time and
   creates your entry in Samba's password database, with a password Samba keeps apart from the
   login password: the one other machines sign in with.
4. Home folders are mode 0700, so the tab says the folder needs extra permissions: **Fix
   Permissions** lets others pass through your home folder (`o+x`), not list it, so `smbd` can
   reach the shared folder.
5. Choose who may read or write it, and **OK**. The share is a usershare under
   `/var/lib/samba/usershares`, and the running `smbd` picks it up at once. With `smbd` down the
   tab reports that `Everyone` cannot be converted to a SID, and no share is made.
6. Open the firewall to other machines:

   ```bash
   kdos-power firewall smb on
   ```

   or turn on **smb** on System Settings → **Firewall**. `smb` opens TCP 445 only.

From a terminal, a member of `sambashare` makes the same share with
`net usershare add <name> <path> "<comment>" <acl> guest_ok=n` while `smbd` runs, and a share
defined in `smb.conf` below `[global]` is served too; `sudo service start samba` starts `smbd`
when nothing else has. `testparm -s` checks
the file, and `smbclient -L localhost -U <you>` lists what is served.

On the live medium, the shares, the group membership and the Samba password under
`/var/lib/samba/private` are all held in memory and are gone at the next reboot, unless the medium
has a persistence store ([Live persistence](live-persistence.md)).

### Serving a folder over NFS

1. Add the export to `/etc/exports`, in the form `exports(5)` describes, for example
   `/srv/share 192.168.1.0/24(rw,sync,no_subtree_check)`.
2. Start the server:

   ```bash
   sudo service start nfsd
   ```

   If it is already running, `sudo exportfs -r` applies the edited file instead.

3. Open the firewall:

   ```bash
   kdos-power firewall nfs on
   ```

   `nfs` opens TCP 2049. The server speaks NFSv4 only, so no portmapper port is needed;
   `cat /proc/fs/nfsd/versions` prints `-3 +4 +4.1 +4.2` while it runs.

On the live medium the root filesystem is an overlay, which the kernel's NFS server cannot
export, and an NFSv4 client reaches every export through it: a mount of any export below `/`
fails with *mount.nfs4: mounting <server>:/srv/share failed, reason given by server: No such file
or directory*, whatever filesystem the export is on. An export of a filesystem of its own, such as
a mounted disk, made the server's root with `fsid=0`, is mounted as `<server>:/`:

```bash
sudo mkdir -p /srv/nfs
sudo mount /dev/sdb1 /srv/nfs
echo '/srv/nfs 192.168.1.0/24(rw,sync,no_subtree_check,fsid=0)' | sudo tee /etc/exports
sudo service start nfsd
```

and on the client, `sudo mount -t nfs4 <server>:/ /mnt/share`. Mounted from this machine itself,
such an export is writable by the account that owns its top directory.

An export below `/` on an installed system, whose root is a real filesystem, has not been tried.

### Syncing a folder with Syncthing

1. On each machine, open **File Sync** from the application menu. It runs `kdos-syncthing` in a terminal
   and serves the web interface on `127.0.0.1:8384`; closing the terminal stops syncing.
2. On each machine, open the firewall name:

   ```bash
   kdos-power firewall syncthing on
   ```

   It opens TCP 22000 and UDP 22000 for syncing and UDP 21027 for discovery.

3. In the web interface, add the other machine's device ID and share a folder with it.

### Sending one file

1. Send it:

   ```bash
   kdos share report.pdf
   ```

   The window shows a code word, and the same code as a QR code when the window is large enough
   to hold it whole.

2. On the other machine, receive it:

   ```bash
   croc <code>
   ```

**Send with a Code Word** in a file's context menu in Dolphin runs the same command, which then opens a terminal
of its own. Every
option is in [kdos share](../../kdos/04-programs/kdos-command.md#kdos-share).

## Tips

- A `.local` name and a bare NetBIOS name in an `smb://` address are resolved by the network
  view's own discovery; a name you put in `/etc/hosts` is tried first and wins.
- Dolphin's **Share** tab and `net usershare` need membership of `sambashare` (gid 65); see
  [Users and groups](users-and-groups.md).
- `kdos why /etc/init.d/83_samba.sh` names the package that owns a file and the recipe it was built
  from.

## Troubleshooting

**Dolphin's network view lists nothing, or not the machine you want.** The list is whatever
answered an mDNS query, a NetBIOS broadcast and a WS-Discovery probe at that moment. A machine that
is asleep, on another subnet or behind a router that does not forward multicast is missing; type
its name or address instead. Check that Avahi runs (`service status avahi`).

**A Kerberos mount fails with `Required key not available`.** The kernel found no ticket. Check
`klist` as the user who asked for the mount, and run `kinit` again when the ticket has expired.
[`/etc/request-key.d/cifs.spnego.conf`]{.idx} and `cifs.upcall` must both be there.

**`smbd` does not start.** Read what its script printed at boot:

```bash
cat /run/kdos-init.samba.log
```

`[SKIP] samba: no account shares folders` is the normal line on a machine where nobody is in
`sambashare` and nothing is shared: `smbd` starts at the next login of the first member. `[SKIP] samba: no /etc/samba/smb.conf` means the file was removed or emptied.
The file holds the boot's output only; a later `sudo service start samba` prints on the terminal.
A member's login that did not start `smbd` is in that session's log:
`grep io.kdos.kded ~/.local/share/sddm/wayland-session.log` shows whether the module asked (it
does not in a session that is not yet in `sambashare`) and what `kdos-powerd` answered, and a
refusal raises a *Folder sharing unavailable* notification. Otherwise run
`testparm -s`, and read the supervisor's log, `/run/kdos-svc.samba.log`, and
`/var/log/samba/log.smbd`.

**The Share tab says sharing is not set up, or a share is refused.** The usershare directory must
be root's, group `sambashare`, mode 1770: `ls -ld /var/lib/samba/usershares`. `sudo service restart
samba` makes it again. Your account must be in `sambashare` in this session: `id`.

**Another machine cannot reach this one's share.** The firewall drops it. `kdos-power firewall
list` shows which names are on; `smb` serves SMB and `nfs` serves NFS.

**`mount -t nfs4` never returns.** The mount is already made: `grep nfs /proc/mounts` lists it and
its files can be used. `/etc/mtab` is missing, and `mount.nfs4` loops trying to update it. Kill it
(`sudo pkill -x mount.nfs4`) and restore the link, `sudo ln -s ../proc/self/mounts /etc/mtab`, or
mount with `-n`, which tells the helper not to record the mount.

**An NFSv3 mount is refused when it asks for locking.** There is no `rpcbind` for `rpc.statd` to
register with. Mount with `-o nolock`.

**Two Syncthing machines do not find each other.** Both need the `syncthing` firewall name on, and
both must be on the same network: global discovery and relays are off in the configuration
`kdos-syncthing` writes.

## Known gaps

- [An NFSv3 mount needs `-o nolock`](../../kdos/06-reference/known-gaps.md#an-nfsv3-mount-needs--o-nolock)
- [Kerberos is built and has never been given a ticket](../../kdos/06-reference/known-gaps.md#kerberos-is-built-and-has-never-been-given-a-ticket):
  a Kerberos share mount and `63_gssd` have not been used against a real domain
- An NFS export below `/` cannot be mounted from a live session; see
  [Serving a folder over NFS](#serving-a-folder-over-nfs)
- Not run on a booted image: Dolphin's Share tab, a `kdos share` transfer, and Syncthing between
  two machines

## See also

- [Firewall](../../kdos/04-programs/kdos-settings.md#firewall): opening the `smb` service, and the
  prompt that offers to let a sharing program in
- [Syncing and serving on the network](../../kdos/02-user-guide/administration.md#syncing-and-serving-on-the-network): Syncthing, mosh and Caddy
- [Services](../../kdos/02-user-guide/administration.md#services): `72_nfsd`, `63_gssd` and `83_samba` in the boot order
- [Opening a service](../../kdos/02-user-guide/administration.md#opening-a-service): every firewall name
- [Avahi and mDNS](avahi-and-mdns.md), [Firewall](firewall.md),
  [Storage and removable media](storage-and-removable-media.md),
  [Live persistence](live-persistence.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 417.* Previous: [416. Wireless regulatory domain](wireless-regulatory-domain.md) · [Contents](../README.md) · Next: [418. Avahi and mDNS](avahi-and-mdns.md)
