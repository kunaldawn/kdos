# The Discover backend. Discover names a backend by its plugin's file name and
# its --backends option appends "-backend" to what it is given, so the plugin
# is kdos-backend.so and `plasma-discover --backends kdos` loads it alone.
#
# Everything but the factory is a static library the plugin and the tests both
# link. The resources are compiled into each binary, because a static
# library's resource initialiser is dropped by the linker when nothing names it.

set(kdos-backend-core_SRCS
    KdosBackend.cpp
    KdosResource.cpp
    KdosTransaction.cpp
    KdosPackd.cpp
)

ecm_qt_declare_logging_category(kdos-backend-core_SRCS
    HEADER libdiscover_kdos_debug.h
    IDENTIFIER LIBDISCOVER_BACKEND_KDOS_LOG
    CATEGORY_NAME org.kdos.discover.backend
    DESCRIPTION "kdos-discover (backend)"
    EXPORT KDOS_DISCOVER
)

add_library(kdos-backend-core STATIC ${kdos-backend-core_SRCS})
set_property(TARGET kdos-backend-core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(kdos-backend-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(kdos-backend-core
    PUBLIC
        kdos-discover-model
        Discover::Common
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::I18n
        KF6::Archive
        KF6::Service
        KF6::KIOGui
        AppStreamQt
)

kcoreaddons_add_plugin(kdos-backend SOURCES KdosPlugin.cpp resources.qrc INSTALL_NAMESPACE "discover")
target_link_libraries(kdos-backend PRIVATE kdos-backend-core)
set_target_properties(kdos-backend PROPERTIES INSTALL_RPATH "${DISCOVER_LIBRARY_DIR}")
