# The tray's plugin. Discover's notifier loads every file in
# discover-notifier/ whose IID is its own DISCOVER_NOTIFIER_IID.

set(KdosNotifier_SRCS
    KdosNotifier.cpp
)

ecm_qt_declare_logging_category(KdosNotifier_SRCS
    HEADER kdos_notifier_debug.h
    IDENTIFIER KDOS_NOTIFIER_LOG
    CATEGORY_NAME org.kdos.discover.notifier
    DESCRIPTION "kdos-discover (tray notifier)"
    EXPORT KDOS_DISCOVER
)

kcoreaddons_add_plugin(KdosNotifier SOURCES ${KdosNotifier_SRCS} INSTALL_NAMESPACE "discover-notifier")
target_link_libraries(KdosNotifier
    PRIVATE
        kdos-discover-model
        Discover::Notifiers
        Qt6::Core
        Qt6::DBus
        KF6::CoreAddons
        KF6::I18n
        KF6::JobWidgets
)
set_target_properties(KdosNotifier PROPERTIES INSTALL_RPATH "${DISCOVER_LIBRARY_DIR}")
