## About
xdg-desktop-portal is the portal front end: it owns `org.freedesktop.portal.Desktop` on the
session bus and forwards each request to the backend `kde-portals.conf` names for it in a Plasma
session, [xdg-desktop-portal-kde](#port-xdg-desktop-portal-kde). It also names the application
behind each request, and the permission store keeps a grant per application id.

## Notes
The camera portal keeps a PipeWire connection whose loop it enters when it attaches the loop's
source to the main context. Upstream leaves the loop only when that source is finalized, which
never happens, because nothing drops the camera's own reference.
`camera-leave-pipewire-loop.patch` leaves the loop and drops the reference in one call, made
before the connection is destroyed. Without it the loop's
recursive mutex is freed while the main thread holds it; musl keeps such a mutex on the holding
thread's robust list, and the next recursive lock on the main thread writes into the freed
memory. The front end then faults in `libc.so` as it exits after losing its bus name or on
SIGHUP, and when PipeWire restarts under it.

The leave is not left to the source's finalize. On a PipeWire restart the camera destroys the
source from inside that source's own dispatch, glib keeps a reference until the dispatch returns,
and a finalize run then would read the connection the camera has already freed: the front end
would fault on every PipeWire restart.

Built without libsystemd, the front end names a host caller's application from the cgroup its
process is in: the deepest component of its cgroup2 path named the way systemd names an
application's unit (`app-<launcher>-<id>-<n>.scope`), parsed by upstream's own unit-name parser,
which `cgroup-app-id.patch` builds whatever the `systemd` option says, so `-Dsystemd=disabled`
stays. Upstream reads the unit with `sd_pid_get_user_unit()` only, so without the patch every host
application has an empty id. With it, a caller still has an empty id unless something placed it
in such a scope, and on KDOS nothing does: Plasma starts an application in the login's own cgroup,
and a box's `app-kdos-<box>-<n>.scope` is never made because elogind leaves the session's
`cgroup.procs` root's. Every caller then shares the empty id and one stored answer; see
[Host applications and boxes reach the portals unidentified](../../../../docs/kdos/06-reference/known-gaps.md#host-applications-and-boxes-reach-the-portals-unidentified).
