## About
ttyper is touch-typing practice in the terminal: it shows a run of words, times you as you type it, and
reports your speed, your accuracy, the words you missed and your worst keys. The port installs the program and a
desktop entry that opens it in a terminal as *Typing Practice*.

## Notes
ttyper is built from a vendor bundle, `ttyper-vendor-<version>.tar.xz`, made by `ports/fetch` with
`cargo vendor`; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring) and
the [rust build shape](../../../../docs/kdos/05-developer/writing-ports.md#rust). Its build is that
shape with nothing added.
