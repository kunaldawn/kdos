## About
[go-md2man]{.idx} converts a Markdown file to a roff manual page. Projects that write their
manual pages in Markdown render them with it at build time, whatever language they are written in:
[podman](#port-podman), [skopeo](#port-skopeo), [conmon](#port-conmon) and
[netavark](#port-netavark) name it in `depends`. It is built with cgo off from its vendor
bundle.

## Notes
The program's own manual page is Markdown too, `go-md2man.1.md`, and `build.sh` renders it with
the binary it has just built.
