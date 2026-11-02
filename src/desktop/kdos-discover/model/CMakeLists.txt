# The readers and the action plans, with no libdiscover type in them, so the
# tests link them without a Discover build. kp_vercmp is libkpkg's own
# comparator, the one kpkg and kdos-packd order versions with.

set(KDOS_LIBS "${CMAKE_SOURCE_DIR}/../../libs")

add_library(kdos-discover-model STATIC
    KdosParse.cpp
    KdosPlan.cpp
    KdosMeter.cpp
    ${KDOS_LIBS}/libkpkg/kp_vercmp.c
)
set_property(TARGET kdos-discover-model PROPERTY POSITION_INDEPENDENT_CODE ON)
set_source_files_properties(${KDOS_LIBS}/libkpkg/kp_vercmp.c PROPERTIES
    COMPILE_OPTIONS "-std=gnu11"
)
target_include_directories(kdos-discover-model
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${KDOS_LIBS}/libkpkg ${KDOS_LIBS}/libkbase ${KDOS_LIBS}/libksig
)
target_link_libraries(kdos-discover-model PUBLIC Qt6::Core KF6::I18n)
