## About

libproxy answers one question for the programs that link it: which proxy, if any, a given URL
should go through. It reads the answer from the `http_proxy`, `https_proxy` and `no_proxy`
environment variables, from Plasma's proxy settings in `kioslaverc`, or from a proxy
auto-configuration (PAC) script, which it fetches and runs. The library is `libproxy.so.1`, its
configuration backend `/usr/lib/libproxy/libpxbackend-1.0.so`, and `proxy` is a command-line
client that prints the proxy for each URL it is given. On KDOS
[signon-ui](#port-signon-ui) links it, to send a provider's sign-in page through the proxy the
desktop is set to.

## Flags

- `-Drelease=true` — Build as a release. Without it the configure tries to install upstream's
  git pre-commit hook into the source tree, which is a tarball with no repository.
- `-Ddocs=false`, `-Dtests=false` — No API reference, which needs the GObject introspection data,
  and no test programs.
- `-Dintrospection=false`, `-Dvapi=false` — No GObject introspection data and no Vala bindings;
  nothing in the tree loads libproxy through either.
- `-Dconfig-env=true` — Read the proxy from the `*_proxy` environment variables, which every
  desktop and a console login can set.
- `-Dconfig-kde=true` — Read Plasma's proxy settings, which System Settings → Network → Proxy
  writes to `kioslaverc`, when `XDG_CURRENT_DESKTOP` is `KDE`.
- `-Dconfig-gnome=false` — No GNOME backend: it reads `org.gnome.system.proxy` through GSettings,
  and the configure then requires `gsettings-desktop-schemas`. The KDOS desktop is Plasma, whose
  settings the KDE backend reads.
- `-Dconfig-sysconfig=false`, `-Dconfig-xdp=false`, `-Dconfig-osx=false`,
  `-Dconfig-windows=false` — No backends for SUSE's `/etc/sysconfig/proxy`, the Flatpak portal,
  macOS or Windows. KDOS has no sysconfig directory and no Flatpak; the last two never apply on
  Linux and are stated so the configure summary says what is built.
- `-Dpacrunner-duktape=true` — Run PAC scripts with [duktape](#port-duktape), the JavaScript engine
  polkit already uses.
- `-Dcurl=true` — Fetch a PAC script from the URL the settings name, through [curl](#port-curl).
  Without it a PAC URL is never read and every request goes direct.
