/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KeyboardPage — the layout the login screen and every session start with
 *
 * An XKB layout and variant, which the installed machine's login screen reads
 * through locale1 and the Keyboard page of System Settings shows; the text
 * consoles get the console map nearest it.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Keyboard")
    complete: Backend.answers.xkbLayout.length > 0
    problem: i18n("Choose a keyboard layout.")

    Kirigami.SearchField {
        id: search
        Layout.fillWidth: true
        placeholderText: i18nc("@info:placeholder", "Search layouts…")
        Accessible.name: i18nc("@label:textbox", "Search keyboard layouts")
    }

    QQC2.Frame {
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 16
        padding: 0

        QQC2.ScrollView {
            anchors.fill: parent

            ListView {
                id: list
                clip: true
                Accessible.name: i18nc("@label:listbox", "Keyboard layouts")
                Accessible.role: Accessible.List
                model: KItemModels.KSortFilterProxyModel {
                    sourceModel: Backend.keyboards
                    filterRoleName: "description"
                    filterString: search.text
                    filterCaseSensitivity: Qt.CaseInsensitive
                }
                delegate: QQC2.ItemDelegate {
                    required property string layout
                    required property string description
                    width: ListView.view.width
                    text: description
                    highlighted: layout === Backend.answers.xkbLayout
                    Accessible.name: highlighted
                        ? i18nc("@info:whatsthis %1 is a keyboard layout", "%1, chosen", description)
                        : description
                    onClicked: {
                        Backend.answers.xkbLayout = layout;
                        Backend.answers.xkbVariant = "";
                    }
                }
            }
        }
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true

        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Layout:")
            text: Backend.keyboards.description(Backend.answers.xkbLayout)
            Accessible.name: i18nc("@info:whatsthis", "Layout: %1", text)
        }

        QQC2.ComboBox {
            id: variant
            Kirigami.FormData.label: i18nc("@label:listbox", "Variant:")
            Layout.fillWidth: true
            readonly property var pairs: Backend.keyboards.variants(Backend.answers.xkbLayout)
            model: {
                const names = [];
                for (let i = 1; i < pairs.length; i += 2) {
                    names.push(pairs[i]);
                }
                return names;
            }
            currentIndex: {
                for (let i = 0; i < pairs.length; i += 2) {
                    if (pairs[i] === Backend.answers.xkbVariant) {
                        return i / 2;
                    }
                }
                return 0;
            }
            popup.width: width
            Accessible.name: i18nc("@label:listbox", "Keyboard variant")
            onActivated: index => Backend.answers.xkbVariant = pairs[index * 2]
        }

    }
}
