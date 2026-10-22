## About

KConfig is KDE's configuration system: `KConfig` and `KSharedConfig` read and write the INI-style
files under `~/.config` and `/etc/xdg`, with cascading from system to user files; `kconfig_compiler`
generates typed settings classes from an XML description at build time; and `kreadconfig6` and
`kwriteconfig6` read and write a key from the shell. `~/.config/kdeglobals`, the file KDE programs share
their settings in and the one `kdos theme` writes (see
[A graphical application](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-graphical-application)),
is read through it.
Nearly every KDE port in the tree depends on it.
