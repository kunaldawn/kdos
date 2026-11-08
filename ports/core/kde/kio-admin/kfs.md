## About

kio-admin is the `admin:/` KIO worker. Dolphin's "Open as Administrator" action opens a folder as
`admin:/path`, and every listing, read, write, rename, permission change and delete under it is
carried out by a helper running as root, after polkit has authorised the user.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The helper, `/usr/lib/libexec/kf6/kio-admin-helper`, owns `org.kde.kio.admin` on the system bus.
Its D-Bus service file carries `User=root` and no `SystemdService=` line, so
[dbus](#port-dbus) starts it on the first call through its setuid activation helper; the
`system.d` policy the package installs lets any user send to the helper's interfaces and only root
own the name.

Every call is checked against the polkit action `org.kde.kio.admin.commands`, which is
`auth_admin_keep` for an active session and `no` for an inactive one, with no `allow_any`. It
therefore works only in a login that elogind registers as active, with an authentication agent
running to ask for the password; polkit-kde-agent-1 is that agent in the Plasma session, and
outside a session every request is refused. The agent is a session port, so it is not in this
port's `depends`.
