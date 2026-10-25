## About
doggo is a command-line DNS client in Go, in the manner of `dig`: it sends a query over UDP, TCP,
DoT, DoH, DoQ or DNSCrypt and prints the answer as a table or as JSON. It is built from its
[Go vendor bundle](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)
with cgo off.

## Flags
- `-X` — sets the string variable `main.buildVersion` at link time to `v$version`. doggo prints
  it for `doggo --version` and puts it in the User-Agent it sends to the Globalping service; left
  unset it reads `unknown`. Upstream's own makefile sets the same variable.

## Notes
`build.sh` runs the binary it has just built, `./doggo completions bash|zsh|fish`, to write the
three shell completion files it installs; doggo handles the `completions` word before it reads any
configuration.
