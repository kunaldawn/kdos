import QtQuick
import QtQuick.Controls as QQC2

QQC2.CheckBox {
    text: "fixture"
    // A CheckBox has no such property; qmllint reports it at the suite's level.
    kdosNoSuchProperty: true
}
