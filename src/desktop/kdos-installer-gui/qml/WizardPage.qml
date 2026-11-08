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

    ColumnLayout {
        id: column
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing
    }
}
