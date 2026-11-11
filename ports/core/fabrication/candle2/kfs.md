## About

Candle2 sends G-code to a CNC machine or laser running GRBL over a serial port, and shows the
toolpath in a 3D view while the job runs, with jogging, work coordinates, height maps for uneven
stock and user command buttons. It is a Qt 5 program built with `qmake`.

## Notes

The desktop entry runs `env QT_QPA_PLATFORM=xcb candle2`, so the program runs under Xwayland
rather than on Qt's Wayland platform; like Brasero's, the entry forces X11 because the native path
is broken (see
[Xwayland and X11 applications](../../../../docs/kfs/16-wiki/xwayland-and-x11-apps.md)).
Candle2 draws its 3D view with a widget derived from `QGLWidget`, Qt 5's legacy OpenGL
widget, whenever Qt is built for desktop OpenGL rather than OpenGL ES, and Qt 5's Wayland platform
composites that widget as transparent: on Wayland the view shows whatever is behind the window.
Under xcb the same widget draws the toolpath. The window's X11 instance name is `candle2`, which
is the entry's `StartupWMClass` and how KWin finds the window's icon. Started as plain `candle2`
from a prompt in the session, the program runs on Wayland with the empty view.
