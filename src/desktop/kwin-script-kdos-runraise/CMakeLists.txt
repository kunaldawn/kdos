# SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)
project(kwin-script-kdos-runraise VERSION 1.0.0 LANGUAGES NONE)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
find_package(KF6Package 6.0 REQUIRED)

option(BUILD_TESTING "Build the tests" ON)

# Every program a key raises has the desktop entry that opens it, and every
# entry has its key: a key with no entry raises but never opens, and an entry
# with no key is a Shortcuts page row that raises nothing.
file(STRINGS package/contents/ui/main.qml raise_lines REGEX "root\\.raiseOrRun\\(\"[^\"]+\"\\)")
set(programs)
foreach(line IN LISTS raise_lines)
    string(REGEX REPLACE ".*raiseOrRun\\(\"([^\"]+)\"\\).*" "\\1" program "${line}")
    list(APPEND programs ${program})
endforeach()
file(GLOB launchers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/kglobalaccel kglobalaccel/*.desktop)
set(entries)
foreach(launcher IN LISTS launchers)
    string(REGEX REPLACE "^org\\.kdos\\.runraise\\.(.+)\\.desktop$" "\\1" program "${launcher}")
    list(APPEND entries ${program})
endforeach()
list(SORT programs)
list(SORT entries)
if(NOT programs OR NOT programs STREQUAL entries)
    message(FATAL_ERROR "run-or-raise keys (${programs}) and kglobalaccel entries (${entries}) differ")
endif()

kpackage_install_package(package kdos_runraise scripts kwin)
foreach(launcher IN LISTS launchers)
    install(FILES kglobalaccel/${launcher} DESTINATION ${KDE_INSTALL_DATADIR}/kglobalaccel)
endforeach()

if(BUILD_TESTING)
    enable_testing()
    find_program(QMLTESTRUNNER qmltestrunner HINTS /usr/lib/qt6/bin REQUIRED)
    set(RAISE_JS ${CMAKE_CURRENT_SOURCE_DIR}/package/contents/ui/raise.js)
    configure_file(tests/tst_raise.qml.in ${CMAKE_CURRENT_BINARY_DIR}/tests/tst_raise.qml @ONLY)
    add_test(NAME runraise-match
             COMMAND ${QMLTESTRUNNER} -platform offscreen -input ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
