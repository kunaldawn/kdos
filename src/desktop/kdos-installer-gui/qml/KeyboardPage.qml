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
 *
 * The list opens on the chosen layout, centred, and comes back to it when
 * the search is emptied. From the keyboard, Tab reaches the list and the
 * arrow keys choose as they move.
 *
 * The form under the list is one fixed width, so its labels stay put as the
 * layout changes: a field as wide as its text would move them with every
 * layout whose longest variant differs.
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

    /* The chosen layout's row in the filtered list, or -1 when the filter
     * hides it. */
    function chosenRow() {
        const source = Backend.keyboards.indexOf(Backend.answers.xkbLayout);
        if (source < 0) {
            return -1;
        }
        return proxy.mapFromSource(Backend.keyboards.index(source, 0)).row;
    }
    function showChosen() {
        const row = chosenRow();
        list.currentIndex = row;
        if (row >= 0) {
            list.positionViewAtIndex(row, ListView.Center);
        }
    }
    function choose(row) {
        const layout = proxy.data(proxy.index(row, 0), KeyboardModel.LayoutRole);
        if (layout && layout !== Backend.answers.xkbLayout) {
            Backend.answers.xkbLayout = layout;
            Backend.answers.xkbVariant = "";
        }
    }

    onEntered: Qt.callLater(showChosen)

    Kirigami.SearchField {
        id: search
        Layout.fillWidth: true
        placeholderText: i18nc("@info:placeholder", "Search layouts…")
        Accessible.name: i18nc("@label:textbox", "Search keyboard layouts")
        /* After the proxy has filtered, which the same change drives. */
        onTextChanged: Qt.callLater(() => {
            if (search.text.length === 0) {
                page.showChosen();
            } else {
                list.positionViewAtBeginning();
            }
        })
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
                activeFocusOnTab: true
                keyNavigationEnabled: true
                Accessible.name: i18nc("@label:listbox", "Keyboard layouts")
                Accessible.role: Accessible.List
                onCurrentIndexChanged: {
                    if (activeFocus && currentIndex >= 0) {
                        page.choose(currentIndex);
                    }
                }
                onActiveFocusChanged: {
                    if (activeFocus && currentIndex < 0 && count > 0) {
                        currentIndex = 0;
                    }
                }
                model: KItemModels.KSortFilterProxyModel {
                    id: proxy
                    sourceModel: Backend.keyboards
                    filterRoleName: "description"
                    filterString: search.text
                    filterCaseSensitivity: Qt.CaseInsensitive
                }
                delegate: QQC2.ItemDelegate {
                    required property string layout
                    required property string description
                    required property int index
                    width: ListView.view.width
                    text: description
                    highlighted: layout === Backend.answers.xkbLayout
                    Accessible.name: highlighted
                        ? i18nc("@info:whatsthis %1 is a keyboard layout", "%1, chosen", description)
                        : description
                    onClicked: {
                        list.currentIndex = index;
                        page.choose(index);
                    }
                }
            }
        }
    }

    Kirigami.FormLayout {
        id: form
        Layout.fillWidth: true

        readonly property real fieldWidth: Kirigami.Units.gridUnit * 20

        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Layout:")
            Layout.preferredWidth: form.fieldWidth
            Layout.maximumWidth: form.fieldWidth
            elide: Text.ElideRight
            text: Backend.keyboards.description(Backend.answers.xkbLayout)
            Accessible.name: i18nc("@info:whatsthis", "Layout: %1", text)
        }

        QQC2.ComboBox {
            id: variant
            Kirigami.FormData.label: i18nc("@label:listbox", "Variant:")
            Layout.preferredWidth: form.fieldWidth
            Layout.maximumWidth: form.fieldWidth
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
