/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — progress read from a tool's own lines
 * ---------------------------------
 */

#include "KdosMeter.h"

#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace Kdos
{

StepMeter::StepMeter(Meter kind, int units)
    : m_kind(kind)
    , m_units(qMax(1, units))
{
}

bool StepMeter::feed(const QString &line)
{
    static const QRegularExpression building(u"^==> building \\S+$"_s);
    static const QRegularExpression already(u"^\\s+\\S+ is already here$"_s);
    static const QRegularExpression installed(u"^==> (\\S+) installed$"_s);
    static const QRegularExpression imported(u"^==> ([A-Za-z0-9_][A-Za-z0-9._-]*)$"_s);
    static const QRegularExpression importDone(u"^==> \\d+ imported, \\d+ failed$"_s);

    const int before = m_done;

    switch (m_kind) {
    case Meter::None:
        break;
    case Meter::AppboxInstall:
        if (building.match(line).hasMatch() || already.match(line).hasMatch()) {
            // The last unit is the box and the launchers, which only the
            // closing line accounts for.
            m_done = qMin(m_done + 1, m_units - 1);
        } else if (installed.match(line).hasMatch()) {
            m_done = m_units;
            m_finished = true;
        }
        break;
    case Meter::AppboxSet:
        if (installed.match(line).hasMatch())
            m_done = qMin(m_done + 1, m_units);
        break;
    case Meter::AppboxImport:
        if (importDone.match(line).hasMatch()) {
            m_done = m_units;
            m_finished = true;
        } else if (imported.match(line).hasMatch()) {
            m_done = qMin(m_done + 1, m_units);
        }
        break;
    }
    return m_done != before;
}

}
