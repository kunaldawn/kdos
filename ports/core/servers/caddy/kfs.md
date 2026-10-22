## About

Caddy is a web server and reverse proxy, written in Go, that manages its own TLS certificates. It
builds as one static binary with its standard modules compiled in. The shipped `Caddyfile`
serves [kiwix-tools](#port-kiwix-tools)' `kiwix-serve`, the offline library, expected on port 8080, over HTTPS on port
8443 to any browser on the LAN. Caddy is started by hand with
`caddy run --config /etc/caddy/Caddyfile`, and other machines reach it once the firewall's `caddy`
name is on (see
[Syncing and serving on the network](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#syncing-and-serving-on-the-network)).

## Notes

Caddy's default is to ask Let's Encrypt for every site's certificate. On a machine with no route
to the internet that fails at every start, so the shipped `Caddyfile` uses `tls internal` and
`local_certs`: the certificates come from Caddy's own local CA, which needs nothing outside the
machine, and a browser's warning is answered by trusting that one root rather than by turning TLS
off. `auto_https disable_redirects` turns off Caddy's automatic HTTP-to-HTTPS redirects. A private CA shared by several machines is [step-ca](#port-step-ca)'s job; Caddy can renew
against it over ACME.

The Go modules come from the vendor bundle, `caddy-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `go mod vendor` because the recipe says `vendoring = go`; `build.sh` unpacks it and
builds with `-mod=vendor`, offline. See
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring).

The manual pages are written by the binary just built: `caddy manpage --directory` generates one
page for each Caddy command, in section 8.
