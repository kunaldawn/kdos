labwc_sources = files(
  'kdos-appid.c', # KDOS
  'kdos-config.c', # KDOS
  'kdos-child.c', # KDOS
  'kdos-wallpaper.c', # KDOS
  'kdos-frames.c', # KDOS
  'kdos-cmd.c', # KDOS
  'kdos-winpos.c', # KDOS
  'kdos-group.c', # KDOS
  'kdos-idle.c', # KDOS
  'kdos-layerfocus.c', # KDOS
  'kdos-lid.c', # KDOS
  'kdos-peek.c', # KDOS
  'kdos-thumb.c', # KDOS
  'kdos-crt.c', # KDOS
  'action.c',
  'buffer.c',
  'debug.c',
  'desktop.c',
  'dnd.c',
  'edges.c',
  'idle.c',
  'interactive.c',
  'layers.c',
  'magnifier.c',
  'main.c',
  'node.c',
  'output.c',
  'output-state.c',
  'output-virtual.c',
  'overlay.c',
  'placement.c',
  'regions.c',
  'resistance.c',
  'resize-outlines.c',
  'seat.c',
  'server.c',
  'session-lock.c',
  'show-desktop.c',
  'snap-constraints.c',
  'snap.c',
  'tearing.c',
  'theme.c',
  'view.c',
  'view-impl-common.c',
  'window-rules.c',
  'workspaces.c',
  'xdg.c',
  'xdg-popup.c',
)

if have_xwayland
  labwc_sources += files(
    'xwayland.c',
    'xwayland-unmanaged.c',
  )
endif

if have_libsfdo
  labwc_sources += files(
    'desktop-entry.c',
  )
endif

subdir('common')
subdir('config')
subdir('cycle')
subdir('decorations')
subdir('foreign-toplevel')
subdir('img')
subdir('input')
subdir('menu')
subdir('scaled-buffer')
subdir('ssd')
