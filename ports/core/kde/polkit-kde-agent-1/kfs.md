## About

The polkit KDE agent is the Plasma session's polkit authentication agent: when a program asks
[polkit](#port-polkit) for an action whose policy is `auth_admin` or `auth_self`, polkit asks the
agent registered for the caller's session, and this agent shows the password dialog and answers.
It registers for the session it is started in, so it serves every program in that login session;
without an agent such a request is refused.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compile none of the interface translation catalogues in `po/`.
  kpkg removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The agent registers for `UnixSessionSubject(getpid())`, so polkit must be able to map its process to
a login session. That needs polkit built with session tracking and a running `elogind`; when the
lookup fails, the registration fails and the agent exits with status 1, and every later
`auth_admin` request in the session is refused with no dialog.

`plasma_session` starts the agent from the XDG autostart entry
`/etc/xdg/autostart/polkit-kde-authentication-agent-1.desktop`, phase 0. The install also puts a
systemd user unit under `/usr/lib/systemd`, which the package lint refuses (see
[What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses));
`build.sh` removes the directory. The autostart entry carries `X-systemd-skip=true`, which tells a
systemd session to use the unit instead; a session started without systemd ignores the key and
runs the entry.

[polkit](#port-polkit) is in `depends` for run time: the agent talks to its daemon over the system
bus, and its `polkit-agent-helper-1` checks the password.
