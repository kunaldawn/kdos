# SPDX-FileCopyrightText: 2026 Kunal Dawn <kunal.dawn@gmail.com>
# SPDX-License-Identifier: MIT
#
# The Release Notes entry opens a page this port installs, and Kickoff and
# KRunner find it under the words a person types for it: an Exec naming a
# page that is not in extra-pages opens Welcome Center on its tour instead.
#
#   cmake -D ENTRY=<io.kdos.releasenotes.desktop> -D PAGES=<extra-pages> -P launcher.cmake

if(NOT EXISTS "${ENTRY}")
    message(FATAL_ERROR "no launcher at ${ENTRY}")
endif()
file(STRINGS "${ENTRY}" lines)
set(exec "")
set(keywords "")
set(name "")
foreach(l IN LISTS lines)
    if(l MATCHES "^Exec=(.*)$")
        set(exec "${CMAKE_MATCH_1}")
    elseif(l MATCHES "^Keywords=(.*)$")
        set(keywords "${CMAKE_MATCH_1}")
    elseif(l MATCHES "^Name=(.*)$")
        set(name "${CMAKE_MATCH_1}")
    elseif(l MATCHES "^NoDisplay=true")
        message(FATAL_ERROR "the launcher is hidden from the menu")
    endif()
endforeach()
if(NOT exec MATCHES "^plasma-welcome --pages ([^ ]+)$")
    message(FATAL_ERROR "Exec is not one plasma-welcome page: '${exec}'")
endif()
set(page "${CMAKE_MATCH_1}")
if(NOT EXISTS "${PAGES}/${page}")
    message(FATAL_ERROR "Exec opens ${page}, which is not in extra-pages")
endif()
file(READ "${PAGES}/${page}" qml)
if(NOT qml MATCHES "kdos\", \"update\", \"notes\"")
    message(FATAL_ERROR "${page} does not read `kdos update notes`")
endif()
if(NOT name STREQUAL "Release Notes")
    message(FATAL_ERROR "Name is '${name}', not 'Release Notes'")
endif()
string(TOLOWER "${keywords}" kw)
foreach(word "release notes" "what's new")
    string(FIND "${kw}" "${word}" at)
    if(at LESS 0)
        message(FATAL_ERROR "Keywords lack '${word}': '${keywords}'")
    endif()
endforeach()
