## About
xdg-desktop-portal is the portal front end: it owns `org.freedesktop.portal.Desktop` on the
session bus and forwards each request to the backend `kdos-portals.conf` names for it, the KDOS,
wlr and GTK backends on this desktop. `kdos-desktop-start` has the bus start it once the capture
backend owns its name; see
[Startup ordering](../../../../docs/kdos/03-architecture/session.md#startup-ordering).

## Notes
The camera portal keeps a PipeWire connection whose loop it enters when it attaches the loop's
source to the main context. Upstream leaves the loop only when that source is finalized, which
never happens, because nothing drops the camera's own reference. The patch leaves the loop and
drops the reference in one call, made before the connection is destroyed. Without it the loop's
recursive mutex is freed while the main thread holds it; musl keeps such a mutex on the holding
thread's robust list, and the next recursive lock on the main thread writes into the freed
memory. The front end then faults in `libc.so` as it exits after losing its bus name or on
SIGHUP, and when PipeWire restarts under it.

The leave is not left to the source's finalize. On a PipeWire restart the camera destroys the
source from inside that source's own dispatch, glib keeps a reference until the dispatch returns,
and a finalize run then would read the connection the camera has already freed: the front end
would fault on every PipeWire restart.
