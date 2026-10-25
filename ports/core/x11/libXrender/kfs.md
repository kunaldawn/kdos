## About

libXrender is the client library for the X RENDER extension: Porter-Duff compositing of
pictures, anti-aliased glyphs and gradients on the server side. cairo's X11 backend,
[libXft](#port-libXft) and [libXcursor](#port-libXcursor) draw through it, so it is in the
closure of nearly every X client that runs on Xwayland. See [No Xorg server, and one carve-out](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).
