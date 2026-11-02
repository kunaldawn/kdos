# The tray entry: org.kdos.checklist, whose QML imports the module above.
plasma_add_applet(org.kdos.checklist
    QML_SOURCES
        qml/main.qml
    GENERATE_APPLET_CLASS
)
