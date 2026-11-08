## About

KQuickCharts is a QML module, `org.kde.quickcharts`, of charts drawn on the GPU: line, bar and pie
charts, with axis labels, legends and grid lines in its `controls` part, fed from Qt item models or
plain value sources. Each chart is a scene graph node with its own shaders, so a chart that updates
every second costs little. The graphs of Plasma's System Monitor and of
[libksysguard](#port-libksysguard)'s sensor faces are drawn with it.

## Notes

The configure requires Qt Qml, Qt Quick, Qt Quick Controls 2 and Qt Shader Tools: the module's
shaders are compiled at build time with `qsb` from [qt6-qtshadertools](#port-qt6-qtshadertools).
The example programs (`BUILD_EXAMPLES`) are off by default upstream and stay off. The source
carries no translations, so the recipe has no `KF_SKIP_PO_PROCESSING`.
