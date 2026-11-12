## About

KRdp is Plasma's Remote Desktop Protocol server. The library takes the session's screen through
[kpipewire](#port-kpipewire) and the screen-cast portal, encodes it and serves it with
[freerdp](#port-freerdp)'s server library, and feeds the client's keyboard and pointer back
through [libei](#port-libei). `krdpserver` is the program that does this for a logged-in user,
and the "Remote Desktop" page of System Settings configures it; KDE Connect's virtual-monitor
plugin links the library to put a tablet beside the screen as a second output.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of the server and its settings
  page, which the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_EXAMPLES=OFF` — No `krdpstreamer`, an example server upstream builds by default.
- `-D BUILD_PLASMA_SESSION=ON` — Builds `krdpserver --plasma`, which streams the screen through
  KWin's own screen-cast protocol from
  [plasma-wayland-protocols](#port-plasma-wayland-protocols) instead of the portal; KWin grants
  that protocol to the server through the `X-KDE-Wayland-Interfaces` line of its desktop entry,
  so no portal permission dialog stands in front of it. It is upstream's default, named so the
  dependency it brings stays in `depends`.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Systemd=ON` — Keep libsystemd out. With it, the settings page
  reads the server's last log lines from the journal; KDOS has no journal, and a `libsystemd.pc`
  in the build root would otherwise turn that code on.

## Patches

`kcm-no-systemd.patch` lets the settings page run the server without a systemd user manager; it is
to be offered upstream as a merge request against `plasma/krdp`. Upstream's page starts, stops,
enables and reads `krdpserver` only through `org.freedesktop.systemd1` on the session bus, which
nothing on KDOS provides, so it would hide its on/off switch and show "Systemd not found". With the
patch `krdpserver` owns the unique D-Bus name `org.kde.krdp-server` through KDBusService, and when
`systemd1` is absent the page starts `/usr/bin/krdpserver` itself, stops it by calling `quit` on
that name's `/MainApplication`, shows it running while the name is owned, and turns "start at
login" into `~/.config/autostart/org.kde.krdpserver.desktop`, a copy of the installed desktop
entry, which Plasma's autostart runs at the next login. A second `krdpserver` hands over to the
first and exits, so a login start and a switch never run two servers on one port.

## Notes

The configure requires FreeRDP's server library, `FreeRDP-Server` 3.1 or later, beside the client
libraries; the [freerdp](#port-freerdp) port builds it, with `-DWITH_SERVER=ON`.

The install puts a systemd user unit, `app-org.kde.krdpserver.service`, and a preset file under
`/usr/lib/systemd`; `build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)); the
page manages the server through the patch above instead. `krdpserver` reads the settings the page
writes, and a running server takes changed ones after the page's restart.

With password authentication through PAM turned on in the settings, the server checks the RDP
user's password against the `login` PAM service.

The server needs a TLS certificate and key and refuses to start without them. The settings
page writes both when it opens with automatic certificates on, upstream's default, by running
`openssl req` into `~/.local/share/krdpserver`, so [openssl](#port-openssl) is in `depends` for
run time; without the program no certificate is written and `krdpserver` does not start.
