/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kded-kdos — the decisions
 * ---------------------------------
 */

#include "logic.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace Kdos
{

QString accentFromScheme(const QString &scheme)
{
    static const QRegularExpression re(QStringLiteral("^KDOS([A-Z][a-z]+)$"));
    const QRegularExpressionMatch m = re.match(scheme);

    /* KDOSBox is the neutral title bar of a box with no accent, shipped
     * beside the accents but not one of them. */
    if (!m.hasMatch() || m.captured(1) == QLatin1String("Box")) {
        return QString();
    }
    return m.captured(1).toLower();
}

bool isShippedScheme(const QString &scheme, const QString &userDataDir,
                     const QStringList &dataDirs)
{
    if (scheme.isEmpty() || scheme.contains(QLatin1Char('/'))) {
        return false;
    }
    const QString user = QDir::cleanPath(userDataDir);

    for (const QString &dir : dataDirs) {
        if (QDir::cleanPath(dir) == user) {
            continue;
        }
        const QFileInfo file(dir + QStringLiteral("/color-schemes/") + scheme
                             + QStringLiteral(".colors"));

        if (file.isFile()) {
            return true;
        }
    }
    return false;
}

QString readAccent(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    /* A name and a newline; anything longer than this is not one. */
    const QString text = QString::fromUtf8(file.read(64));
    const QStringList words = text.split(QRegularExpression(QStringLiteral("\\s+")),
                                         Qt::SkipEmptyParts);

    return words.isEmpty() ? QString() : words.first();
}

QStringList ktarSets(const QString &dir, int max, int scanLimit)
{
    QStringList found;
    QDirIterator it(dir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System);
    int seen = 0;

    while (it.hasNext() && seen < scanLimit) {
        it.next();
        ++seen;
        const QFileInfo info = it.fileInfo();

        /* A symlink on a stick can point anywhere on this machine, and an
         * offer to import it would be an offer to import that. */
        if (info.isSymLink() || !info.isFile() || !info.isReadable() || info.size() == 0) {
            continue;
        }
        if (!info.fileName().endsWith(QLatin1String(".ktar"))) {
            continue;
        }
        found.append(info.absoluteFilePath());
    }
    found.sort();
    if (found.size() > max) {
        found = found.mid(0, max);
    }
    return found;
}

bool hasUsershare(const QString &dir, int scanLimit)
{
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot | QDir::System);
    int seen = 0;

    while (it.hasNext() && seen < scanLimit) {
        it.next();
        ++seen;
        const QFileInfo info = it.fileInfo();

        if (info.isSymLink() || !info.isFile()) {
            continue;
        }
        if (info.fileName().startsWith(QLatin1Char(':'))
            || info.fileName().startsWith(QLatin1Char('.'))) {
            continue;
        }
        return true;
    }
    return false;
}

}
