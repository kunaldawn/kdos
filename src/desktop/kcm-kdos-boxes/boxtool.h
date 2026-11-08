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
 *
 * Running kdos-box and kdos-appbox for the Boxes page.
 *
 * THE PAGE NEVER WRITES A PROFILE AND NEVER TALKS TO PODMAN. Every change is
 * the same `kdos-box` command a person types, so a box configured here and
 * one configured at a prompt are the same bytes, and the things only
 * kdos-box knows (which keys podman can enforce, which need the box made
 * again) are never re-derived here.
 *
 * NO SHELL. A program and an argument vector, always: a box name reaches
 * here from a file name, a podman listing or the command line that opened
 * System Settings, and none of those is text a shell may read. A name is
 * also checked against the rule kdos-box itself applies before it is passed,
 * so a refused name is refused here with a sentence rather than by a child.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace Kdos
{

class BoxTool : public QObject
{
    Q_OBJECT

public:
    struct Result {
        int exitCode = -1;
        bool started = false;
        QString output;     /* stdout and stderr, interleaved as written */
        QString lastLine;   /* the last non-empty line, for a status line */
        int step = 0;       /* in a sequence, the index of this command */
    };
    using Callback = std::function<void(const Result &)>;

    /* A query (a listing, a report) only reads, and is killed if the page
     * closes under it. An act changes a box: one already running is left
     * to finish when the page closes, because killing `kdos-box create` or
     * `kdos-appbox export` half-way leaves a half-made box or archive, and
     * the page's window is not held while it does. */
    enum class Kind {
        Query,
        Act,
    };

    explicit BoxTool(QObject *parent = nullptr);
    ~BoxTool() override;

    /* kdos-box's name rule: [A-Za-z0-9._-], under 64 bytes, not starting
     * with '.' or '-'. A name is a container name, a file name and a path
     * component at once. */
    static bool isValidBoxName(const QString &name);

    /* `program` is looked up on $PATH when it runs, so a test puts its own
     * kdos-box first on $PATH and nothing else changes. */
    void run(const QString &program, const QStringList &args, Callback done, Kind kind = Kind::Query);

    /* Runs each step in order as an act, stopping at the first that fails;
     * `done` gets the failing step's result, or the last one's. A page that
     * closes mid-sequence lets the running step finish and starts no more. */
    void runSequence(const QList<std::pair<QString, QStringList>> &steps, Callback done);

    bool isBusy() const;

Q_SIGNALS:
    void busyChanged();
    /* A line of output from the running command, for a progress line. */
    void progress(const QString &line);

private:
    void runFrom(const QList<std::pair<QString, QStringList>> &steps, int index, Callback done);

    int m_running = 0;
};

}
