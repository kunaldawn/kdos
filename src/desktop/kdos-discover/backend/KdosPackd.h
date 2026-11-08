/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-discover — one request to kdos-packd
 *
 * ONE REQUEST PER CONNECTION, the daemon's whole protocol: connect, write a
 * line, read until the daemon closes. Nothing is kept between two requests,
 * so a medium pulled out between them is simply absent from the second
 * answer. The socket is $KDOS_PACKD_SOCKET when set, as kdos-appbox reads it.
 *
 * THE DAEMON SERVES ONE REQUEST AT A TIME and sets no timeout, so a reply
 * that never comes holds this request open; the timer ends it and reports the
 * daemon as not answering rather than leaving Discover waiting.
 * ---------------------------------
 */

#pragma once

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

namespace Kdos
{

class PackdRequest : public QObject
{
    Q_OBJECT
public:
    // Starts at once; finished() fires exactly once, then the object deletes
    // itself.
    static PackdRequest *send(const QString &request, QObject *parent, int timeoutMs = 120000);

    static QString socketPath();

Q_SIGNALS:
    // `reply` is everything the daemon wrote; `error` is set when it could
    // not be asked at all.
    void finished(const QByteArray &reply, const QString &error);

private:
    PackdRequest(const QString &request, QObject *parent, int timeoutMs);
    void done(const QString &error);

    QLocalSocket m_socket;
    QTimer m_timer;
    QByteArray m_request;
    QByteArray m_reply;
    bool m_done = false;
};

}
