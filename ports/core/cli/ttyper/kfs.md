## About
ttyper is touch-typing practice in the terminal: it shows a run of words, times you as you type
it, and reports your speed in raw and accuracy-adjusted words per minute, your accuracy and your
worst keys, with a chart of your speed across the test, then offers a new test made of the words
you missed. The port installs the program and a desktop entry that opens it in a terminal as
*Typing Practice*.

## Notes
ttyper is built from a vendor bundle, `ttyper-vendor-<version>.tar.xz`, made by `ports/fetch` with
`cargo vendor`; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring) and
the [rust build shape](../../../../docs/kdos/05-developer/writing-ports.md#rust). Its build is that
shape with nothing added.
