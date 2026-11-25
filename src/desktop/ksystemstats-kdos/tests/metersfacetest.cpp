/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   metersfacetest — when the KDOS Meters strip steps out of a panel
 *
 * The face is loaded from its source by libksysguard's own controller, with
 * the configuration the KDOS desk's layout.js gives its network meter, and
 * its compact representation is put in a horizontal panel window. The strip
 * steps out while that window is narrower than stepOutBelow grid units and
 * comes back when it is wider, read from the window each time it is
 * resized: a face that read the screen instead would answer the same for
 * every width of one panel. The setting itself is read again from the
 * applet's configuration at each resize, so a value written after the face
 * loaded, as by a desk laid out in the running session, takes effect at the
 * next one.
 */

#include <faces/SensorFaceController.h>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDir>
#include <QFile>
#include <QMetaEnum>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <memory>

class MetersFaceTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QQmlEngine *m_engine = nullptr;
    KSysGuard::SensorFaceController *m_controller = nullptr;
    std::unique_ptr<QQuickWindow> m_window;
    QQuickItem *m_face = nullptr;
    int m_narrow = 0;
    int m_wide = 0;

    void resizePanel(int width)
    {
        m_window->resize(width, 44);
        QTRY_COMPARE(m_window->width(), width);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        /* The face is found the way Plasma finds an installed one: under
         * ksysguard/sensorfaces of a data directory, and in no other. */
        const QString data = m_dir.filePath(QStringLiteral("share"));
        QVERIFY(QDir().mkpath(data + QStringLiteral("/ksysguard/sensorfaces")));
        QVERIFY(QFile::link(QStringLiteral(METERS_FACE_DIR), data + QStringLiteral("/ksysguard/sensorfaces/org.kdos.meters")));
        qputenv("XDG_DATA_HOME", m_dir.filePath(QStringLiteral("home")).toUtf8());
        qputenv("XDG_DATA_DIRS", data.toUtf8());

        auto config = KSharedConfig::openConfig(m_dir.filePath(QStringLiteral("appletsrc")), KConfig::SimpleConfig);
        KConfigGroup applet = config->group(QStringLiteral("Applets")).group(QStringLiteral("1"));
        applet.group(QStringLiteral("Appearance")).writeEntry("chartFace", QStringLiteral("org.kdos.meters"));
        applet.group(QStringLiteral("org.kdos.meters")).group(QStringLiteral("General")).writeEntry("stepOutBelow", 82);

        m_engine = new QQmlEngine(this);
        m_controller = new KSysGuard::SensorFaceController(applet, m_engine, new QQmlEngine(this));
        QCOMPARE(m_controller->faceId(), QStringLiteral("org.kdos.meters"));

        /* The two widths sit a grid unit either side of the threshold, so
         * the test holds whatever font the platform gives Kirigami. */
        QQmlComponent units(m_engine);
        units.setData("import QtQuick\nimport org.kde.kirigami as Kirigami\n"
                      "QtObject { readonly property real gridUnit: Kirigami.Units.gridUnit }",
                      QUrl());
        std::unique_ptr<QObject> unitsObject(units.create());
        QVERIFY2(unitsObject, qPrintable(units.errorString()));
        const qreal gridUnit = unitsObject->property("gridUnit").toReal();
        QVERIFY(gridUnit > 0);
        const qreal threshold = gridUnit * 82;
        m_narrow = int(std::floor(threshold - gridUnit));
        m_wide = int(std::ceil(threshold + gridUnit));

        m_face = m_controller->compactRepresentation();
        QVERIFY(m_face);
        const QMetaObject *meta = m_face->metaObject();
        const QMetaProperty formFactor = meta->property(meta->indexOfProperty("formFactor"));
        QVERIFY(formFactor.isValid() && formFactor.isEnumType());
        bool ok = false;
        const int horizontal = formFactor.enumerator().keyToValue("Horizontal", &ok);
        QVERIFY(ok);
        QVERIFY(m_face->setProperty("formFactor", horizontal));
        QCOMPARE(m_face->property("horizontalFormFactor").toBool(), true);
        QCOMPARE(m_face->property("stepOutBelow").toInt(), 82);

        m_window = std::make_unique<QQuickWindow>();
        resizePanel(m_narrow);
        m_face->setParentItem(m_window->contentItem());
        m_window->show();
    }

    void stepsOutOfANarrowPanel()
    {
        resizePanel(m_narrow);
        QTRY_COMPARE(m_face->property("stepsOut").toBool(), true);
    }

    void staysInAWidePanel()
    {
        resizePanel(m_wide);
        QTRY_COMPARE(m_face->property("stepsOut").toBool(), false);
    }

    /* The same panel narrowed again, with no change of screen. */
    void stepsOutAgainWhenThePanelNarrows()
    {
        resizePanel(m_wide);
        QTRY_COMPARE(m_face->property("stepsOut").toBool(), false);
        resizePanel(m_narrow);
        QTRY_COMPARE(m_face->property("stepsOut").toBool(), true);
    }

    /* A desk laid out in the running session: the face is loaded from
     * chartFace alone and stepOutBelow reaches the applet's configuration
     * afterwards. The next resize reads it. */
    void readsASettingWrittenAfterTheFaceLoaded()
    {
        auto config = KSharedConfig::openConfig(m_dir.filePath(QStringLiteral("appletsrc")), KConfig::SimpleConfig);
        KConfigGroup applet = config->group(QStringLiteral("Applets")).group(QStringLiteral("2"));
        applet.group(QStringLiteral("Appearance")).writeEntry("chartFace", QStringLiteral("org.kdos.meters"));

        auto *controller = new KSysGuard::SensorFaceController(applet, m_engine, new QQmlEngine(this));
        QCOMPARE(controller->faceId(), QStringLiteral("org.kdos.meters"));
        QQuickItem *face = controller->compactRepresentation();
        QVERIFY(face);
        const QMetaObject *meta = face->metaObject();
        const QMetaProperty formFactor = meta->property(meta->indexOfProperty("formFactor"));
        bool ok = false;
        const int horizontal = formFactor.enumerator().keyToValue("Horizontal", &ok);
        QVERIFY(ok);
        QVERIFY(face->setProperty("formFactor", horizontal));
        QCOMPARE(face->property("stepOutBelow").toInt(), 0);

        QQuickWindow window;
        window.resize(m_wide, 44);
        face->setParentItem(window.contentItem());
        window.show();
        QTRY_COMPARE(window.width(), m_wide);
        QCOMPARE(face->property("stepsOut").toBool(), false);

        applet.group(QStringLiteral("org.kdos.meters")).group(QStringLiteral("General")).writeEntry("stepOutBelow", 82);

        window.resize(m_narrow, 44);
        QTRY_COMPARE(window.width(), m_narrow);
        QTRY_COMPARE(face->property("stepsOut").toBool(), true);
        window.resize(m_wide, 44);
        QTRY_COMPARE(window.width(), m_wide);
        QTRY_COMPARE(face->property("stepsOut").toBool(), false);

        face->setParentItem(nullptr);
    }

    void cleanupTestCase()
    {
        if (m_face) {
            m_face->setParentItem(nullptr);
        }
        m_window.reset();
    }
};

QTEST_MAIN(MetersFaceTest)

#include "metersfacetest.moc"
