/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — one request to kdos-packd
 * ---------------------------------
 */

#include "KdosPackd.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

namespace Kdos
{

QString PackdRequest::socketPath()
{
    const QString env = qEnvironmentVariable("KDOS_PACKD_SOCKET");
    return env.isEmpty() ? u"/run/kdos-packd.sock"_s : env;
}

PackdRequest *PackdRequest::send(const QString &request, QObject *parent, int timeoutMs)
{
    return new PackdRequest(request, parent, timeoutMs);
}

PackdRequest::PackdRequest(const QString &request, QObject *parent, int timeoutMs)
    : QObject(parent)
    , m_request(request.toUtf8() + '\n')
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        done(i18n("kdos-packd did not answer"));
    });
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        m_socket.write(m_request);
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, [this] {
        m_reply += m_socket.readAll();
    });
    // What the daemon wrote before it closed is read once more, and only
    // when there is some: reading a socket the peer closed warns.
    auto drain = [this] {
        if (m_socket.bytesAvailable() > 0)
            m_reply += m_socket.readAll();
    };
    connect(&m_socket, &QLocalSocket::disconnected, this, [this, drain] {
        drain();
        done({});
    });
    connect(&m_socket, &QLocalSocket::errorOccurred, this, [this, drain](QLocalSocket::LocalSocketError e) {
        if (e == QLocalSocket::PeerClosedError) {
            drain();
            done({});
            return;
        }
        done(i18n("kdos-packd is not running (%1)", m_socket.errorString()));
    });
    m_timer.start(timeoutMs);
    m_socket.connectToServer(socketPath());
}

void PackdRequest::done(const QString &error)
{
    if (m_done)
        return;
    m_done = true;
    m_timer.stop();
    Q_EMIT finished(m_reply, error);
    m_socket.abort();
    deleteLater();
}

}
