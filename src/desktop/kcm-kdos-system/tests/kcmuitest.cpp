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
 * The shipped plugin, loaded the way System Settings loads it, with its page
 * built against a fixture server.
 *
 * Two things only a running page can show: the QML loads (a missing import or
 * a type error is a blank page in System Settings and no build error), and
 * EVERY CONTROL ON IT HAS AN ACCESSIBLE NAME, which is what Orca reads and
 * what the rig's AT-SPI golden diffs. A control is anything a person can
 * press, type into or choose from; its name is read through QAccessible, the
 * same interface the AT-SPI bridge reads.
 */

#include <KPluginMetaData>
#include <KQuickConfigModule>
#include <KQuickConfigModuleLoader>

#include <QAccessible>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
bool copyTree(const QString &from, const QString &to)
{
    QDir src(from);
    if (!QDir().mkpath(to)) {
        return false;
    }
    const auto entries = src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &fi : entries) {
        const QString dst = to + u'/' + fi.fileName();
        if (fi.isDir() ? !copyTree(fi.filePath(), dst) : !QFile::copy(fi.filePath(), dst)) {
            return false;
        }
    }
    return true;
}

bool nameOwned()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(u"io.kdos.Slots1"_s);
}

bool isControl(QAccessible::Role role)
{
    switch (role) {
    case QAccessible::Button:
    case QAccessible::CheckBox:
    case QAccessible::RadioButton:
    case QAccessible::ComboBox:
    case QAccessible::EditableText:
    case QAccessible::SpinBox:
    case QAccessible::Slider:
    case QAccessible::PageTab:
    case QAccessible::ButtonMenu:
    case QAccessible::ButtonDropDown:
    case QAccessible::Link:
        return true;
    default:
        return false;
    }
}

/* Visible controls with no name, as "role class (parent's name)" lines.
 *
 * A READ-ONLY TEXT EDIT IS NOT A CONTROL: Kirigami.SelectableLabel and a
 * non-editable ComboBox each carry one inside a parent that holds the name
 * (a StaticText, the ComboBox), and nothing can be typed into it. */
void unnamed(QQuickItem *item, QStringList *out, int *controls)
{
    if (!item->isVisible()) {
        return;
    }
    if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item)) {
        const bool readOnlyText = iface->role() == QAccessible::EditableText && iface->state().readOnly;
        if (isControl(iface->role()) && !readOnlyText && !iface->state().invisible) {
            ++*controls;
            if (iface->text(QAccessible::Name).trimmed().isEmpty()) {
                QAccessibleInterface *up = iface->parent();
                *out << u"%1 %2 (in \"%3\")"_s.arg(QString::number(int(iface->role())),
                                                    QString::fromLatin1(item->metaObject()->className()),
                                                    up ? up->text(QAccessible::Name) : QString());
            }
        }
    }
    const auto children = item->childItems();
    for (QQuickItem *child : children) {
        unnamed(child, out, controls);
    }
}
}

class KcmUiTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QProcess m_server;

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("KDOS_SLOTS1_BUS", "session");
        QAccessible::setActive(true);
        QVERIFY2(QDBusConnection::sessionBus().isConnected(), "this suite needs a session bus: run it under dbus-run-session");
        QVERIFY(m_dir.isValid());
        QVERIFY(copyTree(QStringLiteral(FIXTURES_DIR) + u"/ab"_s, m_dir.path()));
        /* An update prepared in slot B, an offer and a stopped job, so every
         * banner and label the page can show before a trial is on it. */
        for (const auto &[name, text] : {std::pair{u"offer"_s, u"0.2.15\n"_s},
                                         std::pair{u"acquired"_s, u"b 0.2.15\n"_s},
                                         std::pair{u"error"_s, u"kpkg: signature check failed\n"_s}}) {
            QFile f(m_dir.path() + u'/' + name);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(text.toUtf8());
        }
        m_server.setProcessChannelMode(QProcess::ForwardedChannels);
        m_server.start(QStringLiteral(FAKESLOTS1_PATH), {m_dir.path()});
        QVERIFY(m_server.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(nameOwned(), 10000);
    }

    void cleanupTestCase()
    {
        m_server.terminate();
        m_server.waitForFinished(5000);
    }

    void pageLoadsAndNamesEveryControl()
    {
        /* A QML error at run time (a binding on an undefined property, a
         * type error in a handler) is a warning, not a failed load; any
         * warning that names a QML file fails the test. */
        QTest::failOnWarning(QRegularExpression(u"\\.qml:\\d+"_s));

        const KPluginMetaData md(QStringLiteral(KCM_PLUGIN_PATH));
        QVERIFY2(md.isValid(), KCM_PLUGIN_PATH);
        QCOMPARE(md.pluginId(), u"kcm_kdos_system"_s);
        QCOMPARE(md.value(u"X-KDE-System-Settings-Parent-Category"_s), u"system-administration"_s);
        // X-DocPath is the book's section under help:/, the path KIO's help
        // worker resolves through /usr/share/doc/HTML/en/kdos and
        // testing/anchorcheck.py holds to a heading of that page.
        QVERIFY(md.value(u"X-DocPath"_s).startsWith(u"kdos/"_s));

        auto engine = std::make_shared<QQmlEngine>();
        const auto result = KQuickConfigModuleLoader::loadModule(md, nullptr, {}, engine);
        QVERIFY2(result.plugin, qPrintable(result.errorText));
        std::unique_ptr<KQuickConfigModule> kcm(result.plugin);

        kcm->load();
        QTRY_VERIFY(kcm->property("configured").toBool());

        QQuickItem *ui = kcm->mainUi();
        QVERIFY2(ui, qPrintable(kcm->errorString()));

        QQuickWindow window;
        window.resize(1100, 1400);
        ui->setParentItem(window.contentItem());
        ui->setSize(window.size());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        /* Both slot forms are built from the server's answer. */
        const auto slotList = kcm->property("slotList").value<QList<QObject *>>();
        QCOMPARE(slotList.size(), 2);
        QTRY_COMPARE(slotList.at(1)->property("version").toString(), u"0.2.13"_s);
        QTRY_VERIFY(kcm->property("updatePrepared").toBool());
        QCOMPARE(kcm->property("offerVersion").toString(), u"0.2.15"_s);
        QTest::qWait(200);

        QStringList missing;
        int controls = 0;
        unnamed(ui, &missing, &controls);
        /* Slot B's start button, the source box, the Discover button and the
         * prepared update's Open Discover. */
        QVERIFY2(controls >= 4, qPrintable(u"only %1 controls found"_s.arg(controls)));
        QVERIFY2(missing.isEmpty(), qPrintable(u"controls without an accessible name: "_s + missing.join(u", "_s)));

        /* With a trial armed the banner's actions are controls too. */
        kcm->metaObject()->invokeMethod(kcm.get(), "startNextTime", Q_ARG(QString, u"b"_s));
        QTRY_VERIFY(kcm->property("trialArmed").toBool());
        QTest::qWait(200);
        missing.clear();
        controls = 0;
        unnamed(window.contentItem(), &missing, &controls);
        /* The banner's Restart Now and Keep replace the start button, and
         * the armed trial replaces the prepared update's banner. */
        QVERIFY2(controls >= 4, qPrintable(u"only %1 controls found"_s.arg(controls)));
        QVERIFY2(missing.isEmpty(), qPrintable(u"controls without an accessible name: "_s + missing.join(u", "_s)));
    }
};

QTEST_MAIN(KcmUiTest)

#include "kcmuitest.moc"
