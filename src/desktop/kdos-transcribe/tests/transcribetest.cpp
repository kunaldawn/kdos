/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   transcribetest — "Transcribe" shows only with a program and a model
 */

#include "transcribe.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Kdos;

namespace
{
void touch(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write("x");
    }
}
} // namespace

class TranscribeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void modelSearchOrder()
    {
        QTemporaryDir t;
        SpeechEnv e;
        e.dataHome = t.path() + QStringLiteral("/home");
        e.systemDirs = {t.path() + QStringLiteral("/usr/share")};
        QVERIFY(kdosWhisperModel(e).isEmpty());

        touch(t.path() + QStringLiteral("/usr/share/whisper.cpp/models/ggml-base.en.bin"));
        QVERIFY(kdosWhisperModel(e).endsWith(QStringLiteral("/usr/share/whisper.cpp/models/ggml-base.en.bin")));

        touch(e.dataHome + QStringLiteral("/whisper.cpp/models/ggml-small.bin"));
        QVERIFY(kdosWhisperModel(e).endsWith(QStringLiteral("/home/whisper.cpp/models/ggml-small.bin")));

        e.modelOverride = t.path() + QStringLiteral("/mine.bin");
        QVERIFY(kdosWhisperModel(e).endsWith(QStringLiteral("/home/whisper.cpp/models/ggml-small.bin")));
        touch(e.modelOverride);
        QCOMPARE(kdosWhisperModel(e), e.modelOverride);
    }

    void offeredOnlyWithEverything()
    {
        QTemporaryDir t;
        SpeechEnv e;
        e.systemDirs = {t.path()};
        e.whisperCli = QStringLiteral("/usr/bin/whisper-cli");
        e.kdos = QStringLiteral("/usr/local/bin/kdos");
        const QStringList audio{QStringLiteral("audio/mpeg"), QStringLiteral("video/webm")};

        QVERIFY(!kdosTranscribeOffered(e, audio, true));
        touch(t.path() + QStringLiteral("/whisper.cpp/models/ggml-tiny.bin"));
        QVERIFY(kdosTranscribeOffered(e, audio, true));

        QVERIFY(!kdosTranscribeOffered(e, audio, false));
        QVERIFY(!kdosTranscribeOffered(e, {QStringLiteral("audio/ogg"), QStringLiteral("text/plain")}, true));
        QVERIFY(!kdosTranscribeOffered(e, {}, true));

        SpeechEnv noCli = e;
        noCli.whisperCli.clear();
        QVERIFY(!kdosTranscribeOffered(noCli, audio, true));
        SpeechEnv noKdos = e;
        noKdos.kdos.clear();
        QVERIFY(!kdosTranscribeOffered(noKdos, audio, true));
    }

    void arguments()
    {
        QCOMPARE(kdosTranscribeArguments({QStringLiteral("/home/u/-odd name.ogg")}),
                 QStringList({QStringLiteral("speech"), QStringLiteral("transcribe"), QStringLiteral("--notify"), QStringLiteral("--"),
                              QStringLiteral("/home/u/-odd name.ogg")}));
    }
};

QTEST_GUILESS_MAIN(TranscribeTest)

#include "transcribetest.moc"
