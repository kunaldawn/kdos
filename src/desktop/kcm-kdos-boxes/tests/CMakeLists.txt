# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Test Widgets)

include(ECMAddTests)

# The tests drive the real kdos-box, built from the tree beside this port by
# kdos-appbox's own build.sh: the files, flags and libraries it compiles are
# the recipe's, so a kdos-box change that breaks the page fails these tests in
# the same change, and a file added to kdos-appbox needs no edit here.
get_filename_component(APPBOX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../system/kdos-appbox ABSOLUTE)
get_filename_component(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs ABSOLUTE)
if(NOT EXISTS ${APPBOX_DIR}/build.sh)
    message(FATAL_ERROR "the Boxes tests drive the real kdos-box: ${APPBOX_DIR} is missing")
endif()
find_program(BASH_EXECUTABLE bash REQUIRED)
file(GLOB appbox_DEPENDS CONFIGURE_DEPENDS
    ${APPBOX_DIR}/build.sh ${APPBOX_DIR}/*.c ${APPBOX_DIR}/*.h ${APPBOX_DIR}/catalogue
    ${LIBS_DIR}/*/*.c ${LIBS_DIR}/*/*.h)
set(appbox_work ${CMAKE_CURRENT_BINARY_DIR}/kdos-appbox)
set(appbox_bin ${appbox_work}/pkg/usr/local/bin/kdos-appbox)
add_custom_command(
    OUTPUT ${appbox_bin}
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${appbox_work}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${appbox_work}/build
    COMMAND ${CMAKE_COMMAND} -E chdir ${appbox_work}/build
            ${CMAKE_COMMAND} -E env PORT_SRC=${APPBOX_DIR} PKG=${appbox_work}/pkg
            ${BASH_EXECUTABLE} -e ${APPBOX_DIR}/build.sh
    DEPENDS ${appbox_DEPENDS}
    COMMENT "Building kdos-appbox with its recipe's build.sh"
    VERBATIM)
add_custom_target(kdos-appbox-real DEPENDS ${appbox_bin})

add_executable(boxfake boxfake.cpp)
target_link_libraries(boxfake Qt6::Core)

set(engine_defs
    KDOS_APPBOX_PATH="${appbox_bin}"
    BOXFAKE_PATH="$<TARGET_FILE:boxfake>"
)

ecm_add_test(boxestest.cpp
    TEST_NAME boxestest
    LINK_LIBRARIES kcm_kdos_boxes_static Qt6::Test
)
target_include_directories(boxestest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(boxestest PRIVATE ${engine_defs})
add_dependencies(boxestest kdos-appbox-real boxfake)

ecm_add_test(kcmuitest.cpp
    TEST_NAME boxes_kcmuitest
    LINK_LIBRARIES KF6::KCMUtilsQuick KF6::CoreAddons Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test Qt6::Widgets
)
target_compile_definitions(boxes_kcmuitest PRIVATE ${engine_defs} KCM_PLUGIN_PATH="$<TARGET_FILE:kcm_kdos_boxes>")
add_dependencies(boxes_kcmuitest kdos-appbox-real boxfake kcm_kdos_boxes)
# The style System Settings runs with; it needs the QApplication that
# linking Qt6::Widgets makes QTEST_MAIN create.
set_tests_properties(boxes_kcmuitest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=org.kde.desktop")
