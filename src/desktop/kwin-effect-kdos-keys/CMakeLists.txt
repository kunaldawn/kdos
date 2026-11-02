# SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)
project(kwin-effect-kdos-keys VERSION 1.0.0 LANGUAGES C)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
find_package(KF6Package 6.0 REQUIRED)

option(BUILD_TESTING "Build the tests" ON)

set(KEYCARD_DIR ${KDE_INSTALL_DATADIR}/kwin/effects/kdos_keys)

add_executable(kdos-keycard-gen keycard-gen.c)
target_compile_options(kdos-keycard-gen PRIVATE -Wall -Wextra)

# The table is compiled into the package, and the package's own
# ShortcutHandler must be placed by a rule above the fallbacks, or the build
# stops: the card's key would otherwise be listed under a group by accident.
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/groups.js
    COMMAND kdos-keycard-gen --check ${CMAKE_CURRENT_SOURCE_DIR}/groups.conf
            ${CMAKE_CURRENT_SOURCE_DIR}/package/contents/ui/main.qml
    COMMAND kdos-keycard-gen ${CMAKE_CURRENT_SOURCE_DIR}/groups.conf
            ${CMAKE_CURRENT_BINARY_DIR}/groups.js
    DEPENDS kdos-keycard-gen groups.conf package/contents/ui/main.qml
    VERBATIM)
add_custom_target(keycard-groups ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/groups.js)

kpackage_install_package(package kdos_keys effects kwin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/groups.js DESTINATION ${KEYCARD_DIR}/contents/ui)
install(FILES groups.conf DESTINATION ${KDE_INSTALL_DATADIR}/kdos RENAME keycard-groups.conf)

if(BUILD_TESTING)
    enable_testing()
    find_program(QMLTESTRUNNER qmltestrunner HINTS /usr/lib/qt6/bin REQUIRED)
    set(KEYS_JS ${CMAKE_CURRENT_SOURCE_DIR}/package/contents/ui/keys.js)
    set(GROUPS_JS ${CMAKE_CURRENT_BINARY_DIR}/groups.js)
    configure_file(tests/tst_keys.qml.in ${CMAKE_CURRENT_BINARY_DIR}/tests/tst_keys.qml @ONLY)
    add_test(NAME keycard-model
             COMMAND ${QMLTESTRUNNER} -platform offscreen -input ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
