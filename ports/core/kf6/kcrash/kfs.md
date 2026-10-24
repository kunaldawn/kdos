## About

KCrash installs a crash handler in a KDE program: on a fatal signal it can restart the program or
hand it to DrKonqi, the KDE crash reporter, and it lets a program mark itself to be restarted
automatically. DrKonqi is not a port, so in KDOS the handler's part is the automatic restart.
Many KDE applications and [kio](#port-kio) link it.

## Flags

- `-D WITH_X11=ON` — Build the X11 integration. For a program under Xwayland, the handler records
  its X display to pass to what it starts after the crash, and closes the crashed program's
  connection to the X server so that a grab it held, of the pointer for one, is released. It is
  upstream's default on Linux.
