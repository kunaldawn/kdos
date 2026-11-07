/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-library-place — put Library in the Places panel, once
 *
 * KIO seeds a new Places list with its own defaults and offers a
 * distribution no way to add one, so this runs at each login and adds
 * library:/ through KIO's own model the first time. A marker,
 * ~/.config/kdos/library-place, records that it was added: a user who
 * removes the place has it stay removed.
 */

#include <KFilePlacesModel>
#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QUrl>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kdos-kio-library");

    const QString marker = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kdos/library-place");
    if (QFileInfo::exists(marker)) {
        return 0;
    }

    const QUrl library(QStringLiteral("library:/"));
    KFilePlacesModel places;
    bool present = false;
    for (int row = 0; row < places.rowCount(); ++row) {
        if (places.url(places.index(row, 0)).matches(library, QUrl::StripTrailingSlash)) {
            present = true;
            break;
        }
    }
    if (!present) {
        places.addPlace(i18ndc("kdos-kio-library", "@item a place in the file manager", "Library"), library, QStringLiteral("folder-library"), QString());
    }

    QDir().mkpath(QFileInfo(marker).absolutePath());
    QFile f(marker);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fprintf(stderr, "kdos-library-place: cannot write %s\n", qPrintable(marker));
        return 1;
    }
    f.write("added\n");
    return 0;
}
