/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   answers — what the pages collect, as kinstall's answer file
 */

#include "answers.h"

#include <QRegularExpression>

namespace Kdos
{

namespace
{
/* One `key = value` line. A value is flattened onto its line whatever it
 * holds: the pages refuse a line break before it gets here, and a line break
 * that got here anyway must not become a second key. */
QString line(const char *key, const QString &value)
{
    QString v = value;
    v.replace(QLatin1Char('\n'), QLatin1Char(' ')).replace(QLatin1Char('\r'), QLatin1Char(' '));
    return QStringLiteral("%1 = %2\n").arg(QLatin1String(key), v);
}

QString flag(bool on)
{
    return on ? QStringLiteral("1") : QStringLiteral("0");
}
}

Answers::Answers(QObject *parent)
    : QObject(parent)
{
}

/*
 * THE KEYS kinstall-core's conf.c reads, and only those: an unknown key is
 * named on kinstall's standard error and installs that setting's default.
 * `keymap` is left out on purpose, so the backend derives the console's map
 * from the XKB layout. `reboot = 0`: the Done page offers the restart, and an
 * install that rebooted by itself would take the page with it.
 */
QString Answers::toText() const
{
    QString t;
    t += line("timezone", QStringLiteral(":/etc/localtime"));
    t += line("timezone_label", m_timezone);
    t += line("xkb_layout", m_xkbLayout);
    t += line("xkb_variant", m_xkbVariant);
    t += line("disk", m_disk);
    t += line("plan", m_plan == QLatin1String("reuse") ? QStringLiteral("reuse") : QStringLiteral("wipe"));
    if (m_plan == QLatin1String("reuse")) {
        t += line("esp", m_esp);
        t += line("root", m_root);
    }
    t += line("format_esp", flag(m_formatEsp));
    t += line("fstype", m_fstype);
    t += line("swap", m_swap);
    t += line("swap_mb", QString::number(m_swapMb));
    t += line("lvm", flag(m_lvm));
    t += line("luks", flag(m_luks));
    if (m_luks) {
        t += line("luks_passphrase", m_luksPassphrase);
    }
    t += line("hostname", m_hostname);
    t += line("oem", flag(m_oem));
    if (!m_oem) {
        t += line("username", m_username);
        t += line("fullname", m_fullname);
        t += line("password", m_password);
        t += line("admin", flag(m_admin));
        t += line("autologin", m_autologin && m_luks ? QStringLiteral("yes") : QStringLiteral("no"));
        t += line("autologin_lock", flag(m_autologinLock));
    }
    t += line("root_locked", flag(m_rootLocked));
    if (!m_rootLocked) {
        t += line("root_password", m_rootPassword);
    }
    t += line("theme", m_theme);
    t += line("desk", m_desk);
    t += line("alien_apps", flag(m_alienApps));
    t += line("apps", m_apps.isEmpty() ? QStringLiteral("none") : m_apps.join(QLatin1Char(' ')));
    t += line("reboot", QStringLiteral("0"));
    return t;
}

bool Answers::fitsAnswerFile(const QString &value)
{
    if (value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'))) {
        return false;
    }
    return value.isEmpty() || (!value.front().isSpace() && !value.back().isSpace());
}

/* conf.c's ki_name_ok: lowercase letters, digits, `-` and `_`, no leading
 * digit, at most 32. */
bool Answers::usernameOk(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^[a-z_-][a-z0-9_-]{0,31}$"));
    return re.match(name).hasMatch();
}

/* conf.c's ki_host_ok: letters, digits and `-`, not starting with `-`. */
bool Answers::hostnameOk(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9-]{0,62}$"));
    return re.match(name).hasMatch();
}

bool Answers::fullnameOk(const QString &name)
{
    for (const QChar c : name) {
        if (c == QLatin1Char(':') || c.unicode() < 0x20 || c.unicode() == 0x7f) {
            return false;
        }
    }
    return true;
}

/* The first word of a full name, folded to what usernameOk accepts: "Ann
 * Lee" is `ann`, "José" is `jose`. Empty when nothing usable is left. */
QString Answers::suggestUsername(const QString &fullname)
{
    const QString first = fullname.trimmed().section(QLatin1Char(' '), 0, 0, QString::SectionSkipEmpty);
    QString out;
    for (const QChar c : first.normalized(QString::NormalizationForm_D)) {
        const QChar l = c.toLower();
        if ((l >= QLatin1Char('a') && l <= QLatin1Char('z')) || (l >= QLatin1Char('0') && l <= QLatin1Char('9'))
            || l == QLatin1Char('_') || l == QLatin1Char('-')) {
            out += l;
        }
    }
    while (!out.isEmpty() && out.front().isDigit()) {
        out.remove(0, 1);
    }
    out.truncate(32);
    return out;
}

void Answers::setAppChosen(const QString &group, bool chosen)
{
    if (chosen == m_apps.contains(group)) {
        return;
    }
    if (chosen) {
        m_apps.append(group);
    } else {
        m_apps.removeAll(group);
    }
    Q_EMIT changed();
}

}
