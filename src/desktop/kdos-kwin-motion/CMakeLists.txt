# SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)
project(kdos-kwin-motion VERSION 1.0.0 LANGUAGES NONE)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
find_package(KF6Package 6.0 REQUIRED)

# Each effect is a KWin/Effect package under share/kwin/effects/<id>, which is
# where KWin's script loader and addFragmentShader() look for it.
foreach(effect kdos_unfold kdos_login kdos_quit)
    kpackage_install_package(${effect} ${effect} effects kwin)
endforeach()

# A window closing is folded by the same shader whether a person or the logout
# closes it.
foreach(effect kdos_unfold kdos_quit)
    install(FILES shaders/fold.frag DESTINATION ${KDE_INSTALL_DATADIR}/kwin/effects/${effect}/contents/shaders)
endforeach()
