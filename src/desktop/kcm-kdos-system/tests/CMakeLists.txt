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

set(slots1_adaptor_SRCS)
qt_add_dbus_adaptor(slots1_adaptor_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../io.kdos.Slots1.xml
    fakeslots.h FakeSlots slots1adaptor Slots1Adaptor)
add_executable(fakeslots1 fakeslots.cpp fakeslots.h ${slots1_adaptor_SRCS})
target_include_directories(fakeslots1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(fakeslots1 Qt6::Core Qt6::DBus)

set(fixture_defs
    FAKESLOTS1_PATH="$<TARGET_FILE:fakeslots1>"
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

ecm_add_test(systemversionstest.cpp
    TEST_NAME systemversionstest
    LINK_LIBRARIES kcm_kdos_system_static Qt6::Test
)
target_include_directories(systemversionstest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(systemversionstest PRIVATE ${fixture_defs})
add_dependencies(systemversionstest fakeslots1)

ecm_add_test(kcmuitest.cpp
    TEST_NAME kcmuitest
    LINK_LIBRARIES KF6::KCMUtilsQuick KF6::CoreAddons Qt6::DBus Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test Qt6::Widgets
)
target_compile_definitions(kcmuitest PRIVATE ${fixture_defs} KCM_PLUGIN_PATH="$<TARGET_FILE:kcm_kdos_system>")
add_dependencies(kcmuitest fakeslots1 kcm_kdos_system)
# The style System Settings runs with; it needs the QApplication that
# linking Qt6::Widgets makes QTEST_MAIN create.
set_tests_properties(kcmuitest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=org.kde.desktop")
