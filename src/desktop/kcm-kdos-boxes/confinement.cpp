/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

#include "confinement.h"

#include <QFile>

#include <unistd.h>

using namespace Qt::StringLiterals;

namespace Kdos
{

namespace
{
Confinement fromWord(const QString &w)
{
    const QString v = w.trimmed().toLower();
    if (v == "confined"_L1 || v == "private"_L1) {
        return Confinement::Confined;
    }
    if (v == "tagged"_L1) {
        return Confinement::Tagged;
    }
    if (v == "unconfined"_L1 || v == "not-confined"_L1 || v == "untagged"_L1) {
        return Confinement::Unconfined;
    }
    return Confinement::Unknown;
}

/* The file is written by kdos-appbox from what a launch saw; a reason that
 * reaches here is shown as text, so it is cut at a sane length and never
 * carries a line break into a banner. */
QString cleanText(const QString &v)
{
    QString out = v.simplified();
    if (out.size() > 300) {
        out = out.left(300);
    }
    return out;
}
}

QString kdosRuntimeDir()
{
    const QString env = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!env.isEmpty()) {
        return env;
    }
    return u"/run/user/%1"_s.arg(getuid());
}

QString confinementStatePath(const QString &box)
{
    return kdosRuntimeDir() + u"/kdos/boxes/"_s + box + u".state"_s;
}

ConfinementRecord parseConfinementRecord(const QString &text)
{
    ConfinementRecord rec;
    bool first = true;
    bool haveVerdict = false;
    const auto lines = text.split(u'\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(u'#')) {
            continue;
        }
        const int eq = line.indexOf(u'=');
        if (eq < 0) {
            if (first && !haveVerdict) {
                rec.state = fromWord(line);
                haveVerdict = true;
            }
            first = false;
            continue;
        }
        first = false;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if ((key == "confinement"_L1 || key == "state"_L1) && !haveVerdict) {
            rec.state = fromWord(value);
            haveVerdict = true;
        } else if (key == "reason"_L1) {
            rec.reason = cleanText(value);
        } else if (key == "launcher"_L1) {
            rec.launcher = cleanText(value);
        } else if (key == "time"_L1) {
            bool ok = false;
            const qint64 t = value.toLongLong(&ok);
            if (ok && t > 0) {
                rec.time = QDateTime::fromSecsSinceEpoch(t);
            }
        }
    }
    return rec;
}

Confinement parseConfinement(const QString &text)
{
    return parseConfinementRecord(text).state;
}

ConfinementRecord readConfinementRecord(const QString &box)
{
    QFile f(confinementStatePath(box));
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return parseConfinementRecord(QString::fromUtf8(f.read(4096)));
}

QString confinementId(Confinement c)
{
    switch (c) {
    case Confinement::Confined:
        return u"confined"_s;
    case Confinement::Tagged:
        return u"tagged"_s;
    case Confinement::Unconfined:
        return u"unconfined"_s;
    case Confinement::Unknown:
        break;
    }
    return QString();
}

}
