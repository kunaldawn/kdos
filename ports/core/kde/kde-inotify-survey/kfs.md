## About

kde-inotify-survey watches the kernel's inotify limits. A kded module notices when the per-user
watch or instance limit is close to exhausted, which leaves file managers, Baloo and editors blind
to changes, and raises a notification that offers to increase it. The `kde-inotify-survey`
command prints which processes hold the watches.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

Raising a limit goes through a KAuth helper, `kded-inotify-helper`, which [kauth](#port-kauth)
starts as root over the system bus after polkit allows the action (`auth_admin` for an active
session). The helper writes `/etc/sysctl.d/50-kde-inotify-survey-max_user_watches.conf` or
`…-max_user_instances.conf` and runs `/usr/sbin/sysctl --system`, the `--system` form that
[procps-ng](#port-procps-ng)'s `sysctl` understands. A boot that applies only `/etc/sysctl.conf`
loses the raised value at the next start, so the boot's sysctl step must read `/etc/sysctl.d` as
well.

The module runs inside `kded6` from [kded](#port-kded), which the Plasma session starts, so
nothing is watched outside a session.
