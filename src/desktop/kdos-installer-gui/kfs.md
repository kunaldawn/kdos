## About

kdos-installer-gui is the graphical installer, Install KDOS: a Kirigami window that walks Welcome
(the time zone), Keyboard, Disk, Encryption, Account, Look, Applications and Summary, then installs
and says how it ended. It is a front end and nothing more. The machine is described to it by
[kinstall](#port-kdos-installer)'s dumps (`kinstall --dump probe|look|apps|plan --json`), and the
install is `pkexec /usr/bin/kinstall --config - --unattended --events json`, with the answers, the
passwords among them, written to kinstall's standard input and the progress read back as one JSON
event per line. The summary page is kinstall's own plan for the same answers, so what it shows is
what runs, and answers kinstall would refuse are refused there. The Disk page's "Partition
Manager…" opens Partition Manager and probes the disks again when it closes. The port is KDOS's own
code with no upstream source, C++ and QML built with CMake against Kirigami, KI18n, KConfig,
KColorScheme and KItemModels; every program it starts is an argument vector given to QProcess,
never a shell.

## Flags

- `-D BUILD_TESTING=OFF` — Build no tests into the package. `testing/plasmatest.sh` configures
  the same sources with tests on: `installertest` reads the answer file the pages produce back as
  kinstall's `key = value` lines, replays event streams kinstall wrote (`tests/fixtures`) through
  the parser in pieces that cut lines, and parses a fixture `evdev.lst`.
- `-D KDOS_KINSTALL=/usr/bin/kinstall` — The binary the installer runs and the pkexec policy
  `io.kdos.installer.run` names by its `exec.path`. pkexec matches the path exactly, so it is
  compiled in and never looked up on `$PATH`; the two must name the same file.

## Notes

The port installs its desktop entry and the live medium's polkit rule under
`/usr/share/kdos/installer`, where nothing reads them, and the policy under
`/usr/share/polkit-1/actions`. On the live medium the boot step `/etc/init.d/63_live-installer.sh`
copies the entry into `/usr/share/applications` and onto the live account's desktop, and the
rule, which grants the install and kpmcore's helper to the local active session with no password,
into `/etc/polkit-1/rules.d`; kinstall leaves all of them out of the copy. An installed machine
therefore offers no "Install KDOS" entry, and running the program there asks for an
administrator's password. The welcome tour's live page opens the same entry: the live medium's
`plasma-welcomerc` names `LiveInstaller=kdos-installer`.

`kinstall` is not in `depends`: it is built by `10_bootstrap` into every tree from the first
bootable one, and no phase list names its port, so a dependency on the port would have `kpkg`
build and install a second copy over it.

A password or passphrase that starts or ends with a space is refused on its page, because
kinstall's answer-file parser trims the ends of every value; a line break cannot be typed into a
field and is flattened if one arrives anyway, so a value can never become a second key.
