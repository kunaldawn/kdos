import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.FormLayout {
    QQC2.CheckBox {
        Kirigami.FormData.label: i18n("Fixture:")
        text: i18n("A control with a name")
        Accessible.name: text
    }
}
