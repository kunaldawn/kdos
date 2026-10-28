## About

KCrash installs a crash handler in a KDE program: on a fatal signal it can restart the program or
hand it to DrKonqi, the KDE crash reporter, and it lets a program mark itself to be restarted
automatically. DrKonqi is not a port, so in KDOS the handler's part is the automatic restart.
Many KDE applications and [kio](#port-kio) link it.

## Flags

- `-D WITH_X11=ON` — Build the X11 integration, which serves DrKonqi alone: for a program under
  Xwayland the handler records its X display to hand to DrKonqi with `--display`, and, for a
  program that asked to keep its file descriptors open, closes its connection to the X server
  before DrKonqi starts, so that a grab it held, of the pointer for one, is released. With no
  DrKonqi installed the handler never takes that path. It is upstream's default on Linux; on, the
  configure requires libX11, which is why [libX11](#port-libX11) is in `depends`.
