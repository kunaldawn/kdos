## About

wev opens a window and prints every Wayland event it receives: pointer motion and buttons,
keyboard keys with their keysyms and modifiers, touch, tablet and the surface's configure
events. It answers "what does the compositor actually send" while a key binding, an input
device or a pointer problem is being worked out, in the way `xev` does on X11.

## Patches

- `release-flags.patch` — Changes the one compile-and-link rule in upstream's `Makefile`: it adds `$(LDFLAGS)` to the command, so the link takes the tree's exported linker flags, and drops the hard-coded `-g`, so no debugging information goes into the installed binary.
