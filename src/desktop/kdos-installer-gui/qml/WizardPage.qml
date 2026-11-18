/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   WizardPage — what every page tells the wizard
 *
 * `complete` is whether Next may leave the page, and `problem` is the one
 * sentence that says why not, shown beside the button. `entered` fires each
 * time the page is shown, which is when a page that depends on another's
 * answers asks the backend again.
 *
 * Whatever takes the keyboard focus inside the page is scrolled into view,
 * with a spacing to spare: Tab onto a field below the fold would otherwise
 * leave the field being typed into hidden under the page's bottom edge.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ScrollView {
    id: page

    property string heading
    property bool complete: true
    property string problem
    default property alias content: column.data

    signal entered()

    contentWidth: availableWidth
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    /* Scroll so that item, a descendant of the page's column, lies wholly
     * inside the viewport; an item taller than the viewport shows its top. */
    function ensureVisible(item) {
        const flick = page.contentItem as Flickable;
        if (!item || !flick) {
            return;
        }
        let up = item;
        while (up && up !== column) {
            up = up.parent;
        }
        if (!up) {
            return;
        }
        const r = item.mapToItem(flick.contentItem, 0, 0, item.width, item.height);
        const spare = Kirigami.Units.largeSpacing;
        let y = flick.contentY;
        if (r.y + r.height + spare > y + flick.height) {
            y = r.y + r.height + spare - flick.height;
        }
        if (r.y - spare < y) {
            y = r.y - spare;
        }
        flick.contentY = Math.max(0, Math.min(y, flick.contentHeight - flick.height));
    }

    Connections {
        target: page.Window.window
        enabled: page.visible
        function onActiveFocusItemChanged() {
            const item = page.Window.activeFocusItem;
            Qt.callLater(page.ensureVisible, item);
        }
    }

    ColumnLayout {
        id: column
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing
    }
}
