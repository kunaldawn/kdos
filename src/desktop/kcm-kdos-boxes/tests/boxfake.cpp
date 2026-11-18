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
 * The container engine under the real kdos-box, for the Boxes tests.
 *
 * ONE PROGRAM, THREE NAMES, dispatched on argv[0] the way kdos-appbox itself
 * is: `podman` answers from a state file of containers and images,
 * `distrobox create` adds a container to it, and any other name only
 * records its arguments (kdos-appbox export, in the tests). Every call is
 * appended to calls.log as one line, the program's name first, so a test
 * asserts on the order of what kdos-box asked for.
 *
 *   $KDOS_FAKE_DIR/podman.state   `container <name> <state> [<manager>]`
 *                                 and `image <ref>` lines; <manager> is
 *                                 the container's `manager` label
 *   $KDOS_FAKE_DIR/calls.log      every call
 *   $KDOS_FAKE_EXIT               what a recorder exits with (default 0)
 *
 * The answers are podman's own shapes for what kdos-box parses: `ps
 * --format` rows with the columns its template names (the name, the state,
 * a creation time of 0 and the `manager` label) in its order, `inspect` the
 * bare status, exit 1 from `container exists` for no such container, and
 * 125 for an inspect of one. `distrobox create` labels its container
 * `manager=distrobox`, as distrobox does.
 */

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>

#include <cstdio>

using namespace Qt::StringLiterals;

namespace
{
QString dir()
{
    return qEnvironmentVariable("KDOS_FAKE_DIR");
}

struct Container {
    QString name;
    QString state;
    QString manager;
};

struct State {
    QList<Container> containers;
    QStringList images;

    int find(const QString &name) const
    {
        for (int i = 0; i < containers.size(); ++i) {
            if (containers.at(i).name == name) {
                return i;
            }
        }
        return -1;
    }
};

State load()
{
    State s;
    QFile f(dir() + u"/podman.state"_s);
    if (!f.open(QIODevice::ReadOnly)) {
        return s;
    }
    const auto lines = QString::fromUtf8(f.readAll()).split(u'\n', Qt::SkipEmptyParts);
    for (const QString &l : lines) {
        const QStringList t = l.simplified().split(u' ');
        if ((t.size() == 3 || t.size() == 4) && t.at(0) == "container"_L1) {
            s.containers.append({t.at(1), t.at(2), t.value(3)});
        } else if (t.size() == 2 && t.at(0) == "image"_L1) {
            s.images << t.at(1);
        }
    }
    return s;
}

void save(const State &s)
{
    QSaveFile f(dir() + u"/podman.state"_s);
    if (!f.open(QIODevice::WriteOnly)) {
        return;
    }
    QTextStream out(&f);
    for (const Container &c : s.containers) {
        out << "container " << c.name << ' ' << c.state;
        if (!c.manager.isEmpty()) {
            out << ' ' << c.manager;
        }
        out << '\n';
    }
    for (const QString &i : s.images) {
        out << "image " << i << '\n';
    }
    out.flush();
    f.commit();
}

void record(const QString &self, const QStringList &args)
{
    QFile f(dir() + u"/calls.log"_s);
    if (f.open(QIODevice::Append)) {
        f.write((QStringList{self} + args).join(u' ').toUtf8() + '\n');
    }
}

void say(const QString &text, FILE *to = stdout)
{
    fputs(text.toUtf8().constData(), to);
    fflush(to);
}

/* The last word that is not a flag or a flag's value: the container. */
QString target(const QStringList &args)
{
    for (auto it = args.crbegin(); it != args.crend(); ++it) {
        if (!it->startsWith(u'-')) {
            return *it;
        }
    }
    return QString();
}

int podman(const QStringList &args)
{
    State s = load();
    const QString verb = args.value(0);

    if (verb == "ps"_L1) {
        const int f = args.indexOf(u"--format"_s);
        const QStringList columns = (f >= 0 ? args.value(f + 1) : u"{{.Names}}\t{{.State}}"_s).split(u'\t');
        for (const Container &c : s.containers) {
            QStringList row;
            for (const QString &col : columns) {
                row << (col.contains(u".Names"_s)       ? c.name
                        : col.contains(u".State"_s)     ? c.state
                        : col.contains(u".Created"_s)   ? u"0"_s
                        : col.contains(u"\"manager\""_s) || col.contains(u".Labels.manager"_s) ? c.manager
                                                        : QString());
            }
            say(row.join(u'\t') + u'\n');
        }
        return 0;
    }
    if (verb == "container"_L1 && args.value(1) == "exists"_L1) {
        return s.find(args.value(2)) >= 0 ? 0 : 1;
    }
    if (verb == "image"_L1 && args.value(1) == "exists"_L1) {
        return s.images.contains(args.value(2)) ? 0 : 1;
    }
    if (verb == "image"_L1) {
        return 1;
    }
    if (verb == "inspect"_L1) {
        const int i = s.find(target(args));
        if (i < 0) {
            say(u"Error: no such object\n"_s, stderr);
            return 125;
        }
        say(s.containers.at(i).state + u'\n');
        return 0;
    }
    const QString name = target(args);
    const int i = s.find(name);
    if (verb == "start"_L1 || verb == "stop"_L1 || verb == "kill"_L1 || verb == "rm"_L1) {
        if (i < 0) {
            say(u"Error: no container with name or ID \"%1\" found\n"_s.arg(name), stderr);
            return 125;
        }
    }
    if (verb == "start"_L1) {
        s.containers[i].state = u"running"_s;
        save(s);
        say(name + u'\n');
        return 0;
    }
    if (verb == "stop"_L1 || verb == "kill"_L1) {
        s.containers[i].state = u"exited"_s;
        save(s);
        say(name + u'\n');
        return 0;
    }
    if (verb == "rm"_L1) {
        if (s.containers.at(i).state == "running"_L1 && !args.contains(u"-f"_s)) {
            say(u"Error: cannot remove container %1 as it is running - running or paused containers cannot be removed without force\n"_s.arg(name), stderr);
            return 2;
        }
        s.containers.removeAt(i);
        save(s);
        say(name + u'\n');
        return 0;
    }
    return 0;
}

int distrobox(const QStringList &args)
{
    if (args.value(0) != "create"_L1) {
        return 0;
    }
    const int n = args.indexOf(u"--name"_s);
    if (n < 0 || n + 1 >= args.size()) {
        return 2;
    }
    State s = load();
    if (s.find(args.at(n + 1)) < 0) {
        s.containers.append({args.at(n + 1), u"created"_s, u"distrobox"_s});
        save(s);
    }
    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString self = QFileInfo(QString::fromLocal8Bit(argv[0])).fileName();
    const QStringList args = app.arguments().mid(1);

    record(self, args);
    if (self == "podman"_L1) {
        return podman(args);
    }
    if (self == "distrobox"_L1) {
        return distrobox(args);
    }
    if (args.value(0) == "export"_L1) {
        say(u"==> flattening %1\n"_s.arg(args.value(2)));
    }
    return qEnvironmentVariableIntValue("KDOS_FAKE_EXIT");
}
