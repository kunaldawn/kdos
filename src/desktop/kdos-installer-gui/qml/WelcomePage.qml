/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   WelcomePage — what this machine is, and where it is
 *
 * The language is English: the image carries no other (the English-only
 * ruling), and the formats for dates, numbers and money are a person's own
 * choice in System Settings afterwards. The time zone is asked here because
 * it is the machine's, not a person's: tzdata's zone1970.tab, which a typed
 * filter narrows, with the zone's local time beside it.
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kdos.installer

WizardPage {
    id: page

    heading: i18nc("@title", "Welcome to KDOS")
    readonly property var disks: Backend.probe.disks || []
    /* The disks kinstall says can take the install. */
    readonly property int usableDisks: {
        let n = 0;
        for (let i = 0; i < disks.length; i++) {
            if (disks[i].fits === true) {
                n++;
            }
        }
        return n;
    }
    readonly property var zones: {
        const f = zoneFilter.text.trim().toLowerCase();
        if (f.length === 0) {
            return Backend.timeZones;
        }
        return Backend.timeZones.filter(z => z.toLowerCase().indexOf(f) >= 0);
    }
    /* One match is the answer: typing "Kolk" chooses Asia/Kolkata. */
    onZonesChanged: {
        if (zoneFilter.text.length > 0 && zones.length === 1) {
            Backend.answers.timezone = zones[0];
        }
    }
    property string clock: Backend.localTime(Backend.answers.timezone)
    complete: disks.length > 0 && Backend.answers.timezone.length > 0
    problem: Backend.busy && disks.length === 0
        ? i18n("Looking at this computer's disks…")
        : disks.length === 0 ? i18n("No disk was found to install on.") : ""

    QQC2.Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: i18n("This puts KDOS on a disk of this computer. Nothing is written until the last page, which shows everything that will happen and asks before it starts.")
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Warning
        visible: Backend.probe.live === false
        text: i18n("This is not the live medium. Installing from a running system copies this system, and any disk chosen later is erased; the disk this system runs from is not offered.")
    }

    Kirigami.FormLayout {
        Layout.fillWidth: true

        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Language:")
            text: i18nc("@info", "English")
            Accessible.name: i18nc("@info:whatsthis", "Language: English")
        }

        Kirigami.SearchField {
            id: zoneFilter
            Kirigami.FormData.label: i18nc("@label:textbox", "Time zone:")
            Layout.fillWidth: true
            placeholderText: i18nc("@info:placeholder", "Filter zones…")
            Accessible.name: i18nc("@label:textbox", "Filter time zones")
        }
        QQC2.ComboBox {
            id: zone
            Layout.fillWidth: true
            model: page.zones
            currentIndex: page.zones.indexOf(Backend.answers.timezone)
            displayText: currentIndex >= 0 ? currentText
                       : page.zones.length === 0 ? i18nc("@item:inlistbox", "No zone matches")
                       : i18nc("@item:inlistbox %1 the chosen zone, not among the matches", "%1 (choose a match)", Backend.answers.timezone)
            enabled: page.zones.length > 0
            popup.width: width
            Accessible.name: i18nc("@label:listbox", "Time zone")
            onActivated: index => Backend.answers.timezone = page.zones[index]
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Local time:")
            text: page.clock || "—"
            Accessible.name: i18nc("@info:whatsthis", "Local time in %1: %2", Backend.answers.timezone, text)
            Timer {
                interval: 15000
                running: page.visible
                repeat: true
                triggeredOnStart: true
                onTriggered: page.clock = Backend.localTime(Backend.answers.timezone)
            }
            Connections {
                target: Backend.answers
                function onChanged() {
                    page.clock = Backend.localTime(Backend.answers.timezone);
                }
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "This computer")
        }

        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Processor:")
            text: Backend.probe.cpu
                ? i18np("%2 (%1 thread)", "%2 (%1 threads)", Backend.probe.cores, Backend.probe.cpu)
                : "—"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            Accessible.name: i18nc("@info:whatsthis", "Processor: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Memory:")
            text: Backend.probe.memory_kb ? Backend.formatSize(Backend.probe.memory_kb * 1024) : "—"
            Accessible.name: i18nc("@info:whatsthis", "Memory: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Firmware:")
            text: Backend.probe.firmware === "uefi"
                ? (Backend.probe.firmware_bits === 32 ? i18n("UEFI (32-bit)") : i18n("UEFI"))
                : Backend.probe.firmware === "bios" ? i18n("Legacy BIOS") : "—"
            Accessible.name: i18nc("@info:whatsthis", "Firmware: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Disks:")
            text: Backend.busy && page.disks.length === 0 ? i18nc("@info probing the disks", "Looking…")
                : i18ncp("@info %2 every disk found", "%1 of %2 can take the install", "%1 of %2 can take the install",
                         page.usableDisks, page.disks.length)
            Accessible.name: i18nc("@info:whatsthis", "Disks: %1", text)
        }
        QQC2.Label {
            Kirigami.FormData.label: i18nc("@label", "Install size:")
            text: Backend.probe.payload_kb ? Backend.formatSize(Backend.probe.payload_kb * 1024) : "—"
            Accessible.name: i18nc("@info:whatsthis", "Install size: %1", text)
        }
    }

    QQC2.BusyIndicator {
        Layout.alignment: Qt.AlignHCenter
        running: Backend.busy && page.disks.length === 0
        visible: running
        Accessible.name: i18nc("@info:whatsthis", "Looking at this computer")
    }
}
