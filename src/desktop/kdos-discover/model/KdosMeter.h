/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — progress read from a tool's own lines
 *
 * kdos-appbox prints one flushed line per step precisely so that a program
 * showing progress can read its standard output and never parse the container
 * engine. The meter counts those lines and nothing else: a line it does not
 * know moves nothing, so a change in podman's chatter can never move the bar.
 * ---------------------------------
 */

#pragma once

#include "KdosPlan.h"

#include <QString>

namespace Kdos
{

class StepMeter
{
public:
    StepMeter(Meter kind, int units);

    // One line of the step's standard output. Returns whether progress moved.
    bool feed(const QString &line);

    int done() const
    {
        return m_done;
    }
    int units() const
    {
        return m_units;
    }
    // A step whose tool reported its own end; the exit status still decides
    // success.
    bool finished() const
    {
        return m_finished;
    }

private:
    Meter m_kind;
    int m_units;
    int m_done = 0;
    bool m_finished = false;
};

}
