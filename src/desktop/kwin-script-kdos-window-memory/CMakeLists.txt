# SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)
project(kwin-script-kdos-window-memory VERSION 1.0.0 LANGUAGES NONE)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
find_package(KF6Package 6.0 REQUIRED)

option(BUILD_TESTING "Build the tests" ON)

kpackage_install_package(package kdos_window_memory scripts kwin)

if(BUILD_TESTING)
    enable_testing()
    find_program(QMLTESTRUNNER qmltestrunner HINTS /usr/lib/qt6/bin REQUIRED)
    set(MEMORY_JS ${CMAKE_CURRENT_SOURCE_DIR}/package/contents/ui/memory.js)
    configure_file(tests/tst_memory.qml.in ${CMAKE_CURRENT_BINARY_DIR}/tests/tst_memory.qml @ONLY)
    add_test(NAME window-memory
             COMMAND ${QMLTESTRUNNER} -platform offscreen -input ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
