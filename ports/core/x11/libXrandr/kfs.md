## About

libXrandr is the client library for the X Resize and Rotate extension, through which an X client
lists outputs and modes, follows monitor changes and sets the screen layout. Under Xwayland the
outputs are the compositor's, so a program on X11 reads the monitor layout through it; the X11
backends of the toolkits use it. See [No Xorg server, and one carve-out](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).
