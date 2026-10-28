## About

Caddy is a web server and reverse proxy, written in Go, that manages its own TLS certificates. It
builds as one static binary with its standard modules compiled in. The shipped `Caddyfile` has one
site: port 8443, which proxies to [kiwix-tools](#port-kiwix-tools)' `kiwix-serve`, the offline
library, expected on `localhost:8080`, and so serves it over HTTPS to any browser on the LAN.
There is no service script; Caddy is started by hand with
`caddy run --config /etc/caddy/Caddyfile`. Other machines reach it once the firewall's `caddy`
name is on (see
[Syncing and serving on the network](../../../../docs/kdos/02-user-guide/administration.md#syncing-and-serving-on-the-network)).
That name opens TCP 8443 and nothing else, so a site moved to another port needs a rule of its own
in `/etc/nftables.d`.

## Notes

For a site with a public domain name, Caddy's default is to get its certificate from a public ACME
CA, Let's Encrypt first. On a machine with no route to the internet every such attempt fails and
the site has no certificate to serve, so the shipped `Caddyfile` uses `local_certs` and
`tls internal`: the certificates come from Caddy's own local CA, which needs nothing outside the
machine, and a browser's warning is answered by trusting that one root rather than by turning TLS
off. `auto_https disable_redirects` turns off Caddy's automatic HTTP-to-HTTPS redirects. A private
CA shared by several machines is [step-ca](#port-step-ca)'s job; Caddy can renew against it over
ACME.

The Go modules come from the vendor bundle, `caddy-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `go mod vendor` because the recipe says `vendoring = go`; `build.sh` unpacks it and
builds with `-mod=vendor`, offline. See
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

The manual pages are written by the binary just built: `caddy manpage --directory` generates one
page for each Caddy command, in section 8.
