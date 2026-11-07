## About

polkit is the system service that other daemons ask whether a user may perform a privileged
action. On KDOS, elogind, NetworkManager, ModemManager, `udisksd`, `accounts-daemon`, `libvirtd`,
`bolt`, `fwupd`, `upower`, `fprintd`, `pcscd`, brltty's braille server and the KAuth helpers of
Plasma and KDE applications ask it, and `pkexec` runs GParted and other programs as root through
it; [networkmanager](#port-networkmanager) and the other ports that ask it depend on this one.

`polkitd` learns from [elogind](#port-elogind) which session a caller belongs to. A caller in the
active local session, the person at the machine, gets each action's `allow_active` answer, a local
session switched away from gets `allow_inactive`, and anything else, an SSH login or a daemon, gets
`allow_any`. Where that answer is a password, polkit-kde-agent-1 asks for it in a Plasma session.
What KDOS decides differently from the shipped defaults is in
`/etc/polkit-1/rules.d/50-kdos.rules`, which comes from the tree's `fs/` overlay rather than from
this port; see [Security model](../../../../docs/kdos/03-architecture/security-model.md).

`polkitd` is started at boot by `/etc/init.d/41_polkitd.sh`, after D-Bus and before
ModemManager and NetworkManager, and never by D-Bus activation; see
[How polkit and ModemManager are started](../../../../docs/kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started).

## Flags

- `--libexecdir=/usr/lib` — Changes nothing in polkit 128, whose build reads no `libexecdir`: it
  installs `polkitd` and `polkit-agent-helper-1` in `lib/polkit-1/` under the prefix whatever the
  option says, so the daemon is `/usr/lib/polkit-1/polkitd`, the path the boot script starts.
- `-Dauthfw=pam` — Authenticates through PAM, from the [pam](#port-pam) port, rather than by
  reading the shadow file itself. It is upstream's default.
- `-Dpam_prefix=/etc/pam.d` — Installs polkit's PAM service file, `polkit-1`, in `/etc/pam.d`,
  beside the `system-*` stacks the pam port writes, instead of upstream's default
  `/usr/lib/pam.d`. Linux-PAM looks a service up in `/etc/pam.d` first, then in `/usr/lib/pam.d`
  and `/usr/share/pam/pam.d`, so a file in `/etc/pam.d` is the one that is read even if another
  copy exists in either of the others.
- `-Dos_type=lfs` — Names the distribution family, which picks the PAM stacks the `polkit-1`
  file includes: `system-auth`, `system-account`, `system-password` and `system-session`, the
  four files the pam port writes. Left empty, polkit's configure guesses from marker files in the
  build root such as `/etc/lfs-release`, finds none, and falls back to including `system-auth`
  for all four.
- `-Dsession_tracking=elogind` — Build the session monitor that asks logind, linked against
  `libelogind` from [elogind](#port-elogind), which is in `depends`. Without a session monitor
  that finds sessions, polkit never sees an active one: every check falls to an action's
  `allow_any` answer, which for most actions is a refusal or a request for the administrator's
  password, so nothing the person at the machine does is granted by the `allow_active` defaults
  and `50-kdos.rules` would have to grant each action by name. `logind` would link libsystemd,
  which is not a port, and `ConsoleKit` asks a daemon that is not installed.
- `-Dintrospection=false` — Generates no GObject introspection data for the polkit libraries, so
  the build does not need [gobject-introspection](#port-gobject-introspection).
- `-Dman=true` — Builds the manual pages (`polkit(8)`, `polkitd(8)`, `pkexec(1)` and the rest)
  from DocBook with `xsltproc`; [libxslt](#port-libxslt), [docbook-xsl](#port-docbook-xsl) and
  [docbook-xml](#port-docbook-xml) are in `depends` for this.
- `-Dgettext=false` — `polkitd` then translates an action's message only from the translations
  written into its `.policy` file, and never looks it up through `dgettext` in a message
  catalogue. It is upstream's default.
- `-Dgtk_doc=false`, `-Dexamples=false`, `-Dtests=false` — Build no API reference, no example
  programs and no test suite. All three are upstream's defaults.

## Notes

The recipe deletes `/usr/lib/systemd`, `/usr/lib/sysusers.d` and `/usr/lib/tmpfiles.d` after the
install. With no systemd to find, polkit's build still installs its unit, account and tmpfiles
fragments at fixed default paths, and none of them is read on KDOS. The account they describe is
made by the port's `postinstall.sh` instead, in the root being installed into; `41_polkitd.sh`
skips the daemon when the account is missing, because `polkitd` drops to it after starting and
would exit at once without it.

The recipe also deletes `/usr/share/dbus-1/system-services/org.freedesktop.PolicyKit1.service`,
the D-Bus activation file the build installs with no option to leave it out. `41_polkitd.sh`
supervises `polkitd`, and with the file a caller that asked for `org.freedesktop.PolicyKit1` before
the supervised daemon held it (ModemManager does as it starts) would make the bus start a second,
unsupervised `polkitd`; whichever lost the name would exit, and a supervised one that lost would be
restarted every five seconds without ever getting it. Without the file the call is refused, and
`41_polkitd.sh` waits until `polkitd` holds the name before the scripts after it run.

The rules files are JavaScript, which `polkitd` runs with [duktape](#port-duktape), the one
JavaScript engine polkit's build accepts.

elogind is built before polkit and needs none of polkit's libraries to build: its
`-Dpolkit=enabled` only installs its policy and makes it ask `polkitd` over D-Bus at run time. So
polkit can depend on elogind with no cycle. At boot, `40_elogind.sh` runs before `41_polkitd.sh`,
so the first checks already find the sessions elogind records.
