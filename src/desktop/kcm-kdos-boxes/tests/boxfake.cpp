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
 *   $KDOS_FAKE_DIR/podman.state   `container <name> <state>` and
 *                                 `image <ref>` lines
 *   $KDOS_FAKE_DIR/calls.log      every call
 *   $KDOS_FAKE_EXIT               what a recorder exits with (default 0)
 *
 * The answers are podman's own shapes for what kdos-box parses: `ps
 * --format` rows as name<TAB>state, `inspect` the bare status, exit 1 from
 * `container exists` for no such container, and 125 for an inspect of one.
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

struct State {
    QList<std::pair<QString, QString>> containers;
    QStringList images;

    int find(const QString &name) const
    {
        for (int i = 0; i < containers.size(); ++i) {
            if (containers.at(i).first == name) {
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
        if (t.size() == 3 && t.at(0) == "container"_L1) {
            s.containers.append({t.at(1), t.at(2)});
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
    for (const auto &[name, state] : s.containers) {
        out << "container " << name << ' ' << state << '\n';
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
        for (const auto &[name, state] : s.containers) {
            say(name + u'\t' + state + u'\n');
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
        say(s.containers.at(i).second + u'\n');
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
        s.containers[i].second = u"running"_s;
        save(s);
        say(name + u'\n');
        return 0;
    }
    if (verb == "stop"_L1 || verb == "kill"_L1) {
        s.containers[i].second = u"exited"_s;
        save(s);
        say(name + u'\n');
        return 0;
    }
    if (verb == "rm"_L1) {
        if (s.containers.at(i).second == "running"_L1 && !args.contains(u"-f"_s)) {
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
        s.containers.append({args.at(n + 1), u"created"_s});
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
