## About

libXcursor loads cursor themes for X clients: it finds a named cursor in the Xcursor theme
directories (such as `~/.icons` and `/usr/share/icons`), reads its images at the size asked for, and
uploads them as a RENDER cursor, an animated one when the cursor has several frames. A program running on Xwayland gets the same named
cursors as the rest of the desktop through it. See [No Xorg server, and one carve-out](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).
