# A HARD FORK OF cage 0.3.1, MIT, pinned and not tracked. See cage.c.
#
# The man page and its scdoc dependency are gone: this system documents itself
# in docs/kdos/, and a second description of the same flags is a second thing
# to keep true.
project('kdos-cage', 'c',
  version: '0.3.1',
  license: 'MIT',
  meson_version: '>=0.58.1',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=true',
  ],
)

add_project_arguments(
  [
    '-DWLR_USE_UNSTABLE',
    '-Wundef',
    '-Wno-unused-parameter',
  ],
  language: 'c',
)

if get_option('buildtype').startswith('debug')
  add_project_arguments('-DDEBUG', language : 'c')
endif

cc = meson.get_compiler('c')

wlroots        = dependency('wlroots-0.20')
wayland_server = dependency('wayland-server')
xkbcommon      = dependency('xkbcommon')
# NAMED, not inherited: embed.c reads the damage region wlroots hands back, and
# a symbol used directly is a library linked directly.
pixman         = dependency('pixman-1')
math           = cc.find_library('m')

have_xwayland = wlroots.get_variable(pkgconfig: 'have_xwayland', internal: 'have_xwayland') == 'true'

conf_data = configuration_data()
conf_data.set10('CAGE_HAS_XWAYLAND', have_xwayland)
conf_data.set_quoted('CAGE_VERSION', meson.project_version())
conf_data.set_quoted('CAGE_UPSTREAM', '0.3.1')

cage_sources = [
  'cage.c',
  'embed.c',
  'idle_inhibit_v1.c',
  'output.c',
  'seat.c',
  'view.c',
  'xdg_shell.c',
  configure_file(input: 'config.h.in',
                 output: 'config.h',
                 configuration: conf_data),
]

# XWayland STAYS. A boxed application may be an X11 client, and the rootless X
# server is this distribution's one carve-out.
if conf_data.get('CAGE_HAS_XWAYLAND', 0) == 1
  cage_sources += 'xwayland.c'
endif

executable(
  meson.project_name(),
  cage_sources,
  dependencies: [
    wayland_server,
    wlroots,
    xkbcommon,
    pixman,
    math,
  ],
  install: true,
)
