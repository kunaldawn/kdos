## About

`kcm-kdos-firewall` is the Firewall page of System Settings, under Networking, and the
notification that offers to let a blocked program in. Both speak to
[kdos-powerd](../../../docs/kdos/04-programs/daemons.md#firewall) over its system-bus face,
`io.kdos.Power1`, and neither knows a port: the page lists the daemon's named services
(`ListFirewallServices`), holds the switches until Apply, and sends one
`SetFirewallService(name, open)` per changed row, in order, stopping at the first refusal. The
interface it relies on is `io.kdos.Power1.xml` beside the sources. Upstream's `plasma-firewall`
is not ported: its backends are firewalld and ufw, and it edits rules by port and protocol, which
the KDOS firewall refuses by design.

The port is KDOS's own code with no upstream source. It builds three things with one CMake
project against KF6 KCMUtils: the KCM plugin `kcm_kdos_firewall`, whose page is QML in `ui/`; the
C program `/usr/libexec/kdos/kdos-fwask`, compiled with `libkbase` from `src/libs` and linked to
basu only; and that program's autostart entry. `kdos-fwask` asks kdos-powerd every ten seconds
how many packets each closed service's counter has turned away
(`ListFirewallKnocks`). When a count rose for KDE Connect, krdp, krfb or kiwix-serve, the
program behind it is running as the session's user, and that user has not been asked about it
on the current NetworkManager connection, it posts one notification offering Allow and Not on
This Network. The answers are kept in `$XDG_STATE_HOME/kdos/fwask`, one line per network and
service. Both follow the daemon's `FirewallServicesChanged` signal: the page reloads its list
when it holds no unapplied change, and `kdos-fwask` asks for the list again and withdraws a
prompt whose service was opened elsewhere while it was up, remembering that as its answer.

## Flags

- `-D KDE_INSTALL_LIBEXECDIR=libexec` — Put `kdos-fwask` in `/usr/libexec/kdos`, beside the
  other helpers KDOS's own ports install there. ECM's default for this tree's layout is
  `lib/libexec`, and the autostart entry names the path the build used.
- `-D KDE_INSTALL_AUTOSTARTDIR=/usr/share/kdos/xdg/autostart` — Install the autostart entry
  among the distribution's defaults instead of `/etc/xdg/autostart`. `/etc/xdg` is the
  administrator's, and the session finds the entry through `XDG_CONFIG_DIRS`, which lists
  `/usr/share/kdos/xdg` after `/etc/xdg`; an administrator disables it by shadowing the file in
  `/etc/xdg/autostart` with `Hidden=true`.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. `testing/plasmatest.sh` configures
  the same sources with tests on and runs them under a private session bus: the page's logic
  and `kdos-fwask` end to end against fakes of `io.kdos.Power1`, polkit, NetworkManager and the
  notification server, and `fwcoretest`, which `testing/selftest.sh` also builds on a bare host.

## Notes

The page goes read-only when polkit, asked without interaction, says no to
`io.kdos.firewall.set` for the page's own bus connection, which is the question the daemon's
check asks of the same caller. The answer "yes after a password" leaves the switches live, and
the daemon's own check raises the prompt when the change is applied. The KCM sets no
`authActionName`: System Settings would then pre-authorise through KAuth on the same click, and
polkit's agent shows one dialog at a time.

`kdos-fwask` asks only about services a session program serves. A rising count on `http` with
no web server running is a scan, and asking about it teaches people to click Allow. Allow opens
the service on every network, because the firewall is one table for the machine; opening it on
one network only would mean closing it again at every change of network, each with a new
password prompt. What is remembered per network is the question, so a person who declined at
a café is asked again at home.

The counters it reads are kdos-powerd's: one named counter for each closed row, read as root
with `nft`. A daemon without `ListFirewallKnocks` answers `UnknownMethod`; the page then shows
no counts and `kdos-fwask` looks again every five minutes; so does a daemon that could not read
the counters, which answers `io.kdos.Power1.Error.Failed`.

The prompt's `Notify` is sent without blocking and waits three minutes for its id, longer than
the session bus's 120-second limit on starting a service. At login the notification server is
still being started when the first knock is seen; a call given up before the bus gives up is still
delivered when Plasma comes, and a second one posted meanwhile would be a notification whose
buttons nobody hears. The service's slot is held from the call to its reply, so a count still
rising posts nothing more; an error reply frees it and the next look asks again.

The notifications carry no `desktop-entry` hint. Plasma asks KWin for a launch token for that
entry on every action, so Not on This Network would show the Firewall page starting in the task
manager while nothing opens. They are named Firewall and wear the `security-high` icon instead,
and are not configured under Settings › Notifications as an application of their own.

A signal names its sender's unique bus name only, and any connection on the system bus may emit
one with the daemon's path and interface. The page's QtDBus connection follows the owner of
`io.kdos.Power1`, and `kdos-fwask` asks the bus who owns the name before it acts on the signal,
so a forged one can neither reset the page nor silence a prompt.
