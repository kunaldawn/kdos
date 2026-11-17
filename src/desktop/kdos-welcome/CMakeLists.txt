# SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22)
project(kdos-welcome VERSION 1.0.0 LANGUAGES NONE)

option(BUILD_TESTING "Build the tests" ON)

set(WELCOME share/plasma/plasma-welcome)

install(FILES intro-customization.desktop DESTINATION ${WELCOME})
file(GLOB PAGES ${CMAKE_CURRENT_SOURCE_DIR}/extra-pages/*.qml)
install(FILES ${PAGES} DESTINATION ${WELCOME}/extra-pages)
file(GLOB PARTS ${CMAKE_CURRENT_SOURCE_DIR}/extra-pages/kdos/*.qml
                ${CMAKE_CURRENT_SOURCE_DIR}/extra-pages/kdos/*.js)
install(FILES ${PARTS} extra-pages/kdos/qmldir DESTINATION ${WELCOME}/extra-pages/kdos)

if(BUILD_TESTING)
    enable_testing()
    find_program(QMLTESTRUNNER qmltestrunner HINTS /usr/lib/qt6/bin REQUIRED)
    set(GROUPS_JS ${CMAKE_CURRENT_SOURCE_DIR}/extra-pages/kdos/groups.js)
    configure_file(tests/tst_groups.qml.in ${CMAKE_CURRENT_BINARY_DIR}/tests/tst_groups.qml @ONLY)
    add_test(NAME groups
             COMMAND ${QMLTESTRUNNER} -platform offscreen -input ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()
