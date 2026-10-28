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
| Mounting an SMB share as a user | [kdos-mountd](#port-kdos-mountd), [cifs-utils](#port-cifs-utils), [kdos-shell](#port-kdos-shell) ([`kdos-connect`]{.idx}) | `58_mountd`, at every boot |
| Mounting with a Kerberos ticket | [krb5](#port-krb5), [cifs-utils](#port-cifs-utils) | the kernel, through `request-key`, on the mount |
| Serving SMB | [samba](#port-samba) | `83_samba`, once [`/etc/samba/smb.conf`]{.idx} exists |
| Mounting and serving NFSv4 | [nfs-utils](#port-nfs-utils) | `72_nfsd`, once [`/etc/exports`]{.idx} names a share; `63_gssd`, once [`/etc/krb5.keytab`]{.idx} exists |
| Syncing folders | [syncthing](#port-syncthing) | **File Sync** in the Start menu, by hand |
| Sending one file | [croc](#port-croc), [kdos-tools](#port-kdos-tools) (`kdos share`) | by hand |
| Shares in GTK file managers | [gvfs](#port-gvfs) | the session bus, on first use |

`58_mountd` is in `fs/etc/init.d`, so every image has it. `72_nfsd`, `63_gssd` and `83_samba` are
written by their ports' `build.sh` and exist only while the port is installed. None of the three
servers is in the installer's list of service choices, so they behave the same on the live medium
and on an installed system: each skips itself at boot, with a `[SKIP]` line, until its
configuration file exists. The lines are in `/run/kdos-init.samba.log`, `/run/kdos-init.nfsd.log`
and `/run/kdos-init.gssd.log`: `[SKIP] samba: no /etc/samba/smb.conf`, `[SKIP] nfsd: no share in
/etc/exports` and `[SKIP] gssd: no /etc/krb5.keytab`. `service status nfsd` names the daemon it
watches, `nfsv4.exportd`. What differs is where the configuration lives. On the live medium
`/etc` is in memory and is lost at power-off unless a persistence store exists
([`kdos persist`](../../kdos/04-programs/kdos-command.md#kdos-persist)); on an installed system it
is on the root filesystem.

## Configuration

### /etc/kdos/mountd.conf

`kdos-mountd` mounts every share it connects with `nosuid`, `nodev` and `noexec`. The image ships
no `mountd.conf`; a line `exec = yes` in it drops `noexec` from removable media and network shares
alike. Only a line that is exactly `exec = yes`, with the blanks around `=` optional, turns it on;
a commented-out `# exec = yes` leaves the setting off. The three settings are in
[/etc/kdos/mountd.conf](../../kdos/04-programs/daemons.md#etckdosmountdconf).

### /etc/request-key.d/cifs.spnego.conf

A Kerberos mount needs the kernel to ask [`cifs.upcall`]{.idx} for a ticket. The krb5 port installs the
rule that joins them:

<!-- kfs:include ports/core/auth/krb5/build.sh lines=60-62 pin=866a2c35ec7c -->

### /etc/samba/smb.conf

The samba port installs no `smb.conf`. Its service script starts [`smbd`]{.idx} only once a non-empty one
exists, and starts `smbd` alone: no `nmbd` and no `winbindd`.

<!-- kfs:include ports/core/servers/samba/build.sh lines=62-92 pin=88369dbfc015 -->

The configure options set where Samba keeps its files: the configuration under [`/etc/samba`]{.idx}, the
password database under `/var/lib/samba/private` and the logs under [`/var/log/samba`]{.idx}:

<!-- kfs:include ports/core/servers/samba/build.sh lines=16-24 pin=087bd4fbb30a -->

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

### Mounting an SMB share from the desktop

1. Open `kdos-connect`: type its name at a prompt, or choose the row *Connect to server…* on the System page
   of Settings. The window lists the shares you have connected. `F1` opens its help page, which is
   also `kdos-doc connect`.
2. Press `b` to list the machines that answered a broadcast, and `Enter` on one to put its name in
   the Server field; `Esc` goes back to the list of connected shares. To type the server
   yourself, press `Enter` on the list of connected shares instead.
3. Fill in Share, User and Domain. `Tab` or `Down` moves to the next field, `Shift+Tab` or `Up` to
   the one before. A server chosen from the broadcast list opens the form on the Password row, so
   move back up to Share first. User is your account on the server; leave Domain empty where
   there is none. `Enter` in any field sends the form.
4. On *Sign in*, `Space` chooses between a password and the Kerberos ticket [`kinit`]{.idx} left in your
   credential cache. Type the password if one is asked for, then press `Enter` to connect.
5. The share is mounted at `/media/<user>/<server>-<share>`, owned by you, with files at mode
   0600 and directories at 0700.
6. To disconnect, select the share in the list and press `d`.

### Mounting an SMB share from a terminal

[`kdos-mount`]{.idx} reaches the same daemon. The verbs that carry a password are reached from
`kdos-connect` only, so a password never appears on a command line; a Kerberos mount carries none
and has a verb here. There is no verb that disconnects a share; do that in `kdos-connect`.

1. List the file servers that answer a broadcast:

   ```bash
   kdos-mount browse
   ```

2. With a ticket, mount a share. `-` stands for "no user" and "no domain", which is the usual case
   with a ticket:

   ```bash
   kinit you@EXAMPLE.ORG
   kdos-mount krb5 fileserver projects - -
   ```

3. List what is mounted:

   ```bash
   kdos-mount shares
   ```

Each verb answers `ok`, followed by its rows, or `err` and a reason, such as `err no machine on
this network answers to the name fileserver`. `kdos-mount` exits 0 whenever the daemon answered,
including when the answer is a refusal, so a script tests the output for a leading `err`.
`kdos-mount` with no verb prints every verb it has. See
[The command-line client](../../kdos/04-programs/daemons.md#the-command-line-client).

Every member of `seat` or `wheel` may use these verbs. As root, `mount.cifs` works directly as on
any Linux system:

```bash
sudo mkdir -p /mnt/share
sudo mount -t cifs //fileserver/projects /mnt/share -o username=you,uid=$(id -u),gid=$(id -g)
```

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

1. Write `/etc/samba/smb.conf`. A minimal standalone server with one share, for the account
   `kdos`, is:

   ```ini
   [global]
      server role = standalone server

   [share]
      path = /srv/share
      read only = no
      valid users = kdos
   ```

2. Create the shared folder, owned by the account, since the server writes into it as that
   account and a folder root owns refuses every write with *Permission denied*:

   ```bash
   sudo install -d -o kdos -g kdos /srv/share
   ```

3. Check the file, and give the account a Samba password (Samba keeps its own; it does not use
   the login password):

   ```bash
   testparm -s
   sudo smbpasswd -a kdos
   ```

   `testparm` prints `Loaded services file OK.` and the file back; `smbpasswd` ends with `Added
   user kdos.`

4. Start the server; it starts at every boot from here on, because the file exists:

   ```bash
   sudo service start samba
   ```

   `smbclient -L localhost -U kdos` lists the share, and the server writes
   `/var/log/samba/log.smbd`.

5. Open the firewall to other machines. This needs membership of `wheel`:

   ```bash
   kdos-power firewall smb on
   ```

   `smb` opens TCP 445 only.

On the live medium, `smb.conf`, the shared folder of step 2 and the Samba password under
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

1. On each machine, open **File Sync** from the Start menu. It runs `kdos-syncthing` in a terminal
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

*Share* in a file's menu runs the same command, which then opens a terminal of its own. Every
option is in [kdos share](../../kdos/04-programs/kdos-command.md#kdos-share).

## Tips

- Typing a server's address or full DNS name into `kdos-connect` needs no broadcast. A `.local`
  name and a bare NetBIOS name are resolved by one, because the C library cannot resolve either;
  a name you put in `/etc/hosts` is tried first and wins.
- GTK applications reach SMB shares through gvfs, as `smb://server/share`. kio-extras is configured
  with `-D CMAKE_DISABLE_FIND_PACKAGE_Samba=ON`, which leaves out its SMB worker, so in Dolphin and
  the other KDE applications mount the share with `kdos-connect` first and open its folder under
  `/media/<user>`.
- A share mounted by `kdos-mountd` belongs to the account that asked for it. Another account on
  the same machine cannot read it.
- `kdos why /etc/init.d/83_samba.sh` names the package that owns a file and the recipe it was built
  from.

## Troubleshooting

**`kdos-connect` or `kdos-mount` answers `err` and a reason.** The reason is the first line of the
mount helper's own message, or the daemon's refusal. A name the daemon cannot resolve is refused
before anything is mounted: check that the server is up, and for a `.local` name that Avahi is
running (`service status avahi`). Every refusal is listed in
[Network shares](../../kdos/04-programs/daemons.md#network-shares).

**`kdos-mount browse` lists nothing, or not the machine you want.** An empty list is the line `ok`
alone. The list is whatever answered
an mDNS and a NetBIOS broadcast at that moment. A machine that is asleep, on another subnet or
behind a router that does not forward broadcasts is missing; type its name or address instead.

**A Kerberos mount fails with `Required key not available`.** The kernel found no ticket. Check
`klist` as the user who asked for the mount, and run `kinit` again when the ticket has expired.
When `cifs.upcall` or [`/etc/request-key.d/cifs.spnego.conf`]{.idx} is missing, the daemon says which.

**A program on a share does not run.** The share is mounted `noexec`. Add `exec = yes` to
`/etc/kdos/mountd.conf`, then disconnect and connect the share again.

**`smbd` does not start.** Read what its script printed at boot:

```bash
cat /run/kdos-init.samba.log
```

`[SKIP] samba: no /etc/samba/smb.conf` means the file is missing or empty. The file holds the
boot's output only; a later `sudo service start samba` prints on the terminal. Otherwise run
`testparm -s`, and read the supervisor's log, `/run/kdos-svc.samba.log`, and
`/var/log/samba/log.smbd`.

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

- [The list of Windows shares on the network is whatever answered a broadcast](../../kdos/06-reference/known-gaps.md#the-list-of-windows-shares-on-the-network-is-whatever-answered-a-broadcast)
- [An NFSv3 mount needs `-o nolock`](../../kdos/06-reference/known-gaps.md#an-nfsv3-mount-needs--o-nolock)
- [Kerberos is built and has never been given a ticket](../../kdos/06-reference/known-gaps.md#kerberos-is-built-and-has-never-been-given-a-ticket):
  `kdos-mount krb5` and `63_gssd` have not been used against a real domain
- An NFS export below `/` cannot be mounted from a live session; see
  [Serving a folder over NFS](#serving-a-folder-over-nfs)
- Not run on a booted image: the `kdos-connect` window, a `kdos share` transfer, and Syncthing
  between two machines

## See also

- [kdos-mountd](../../kdos/04-programs/daemons.md#kdos-mountd): the daemon's verbs, its refusals and how it mounts a share
- [kdos-connect](../../kdos/04-programs/kdos-shell.md#kdos-connect): the network-share window
- [Syncing and serving on the network](../../kdos/02-user-guide/administration.md#syncing-and-serving-on-the-network): Syncthing, mosh and Caddy
- [Services](../../kdos/02-user-guide/administration.md#services): `72_nfsd`, `63_gssd` and `83_samba` in the boot order
- [Opening a service](../../kdos/02-user-guide/administration.md#opening-a-service): every firewall name
- [Avahi and mDNS](avahi-and-mdns.md), [Firewall](firewall.md),
  [Storage and removable media](storage-and-removable-media.md),
  [Live persistence](live-persistence.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 393.* Previous: [392. Wireless regulatory domain](wireless-regulatory-domain.md) · [Contents](../README.md) · Next: [394. Avahi and mDNS](avahi-and-mdns.md)
