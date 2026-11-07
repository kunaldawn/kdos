/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   transcribe — when "Transcribe" is offered, and what it runs
 */

#include "transcribe.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace Kdos
{

SpeechEnv kdosSpeechEnvironment()
{
    SpeechEnv e;
    e.whisperCli = QStandardPaths::findExecutable(QStringLiteral("whisper-cli"));
    e.kdos = QStandardPaths::findExecutable(QStringLiteral("kdos"));
    if (e.kdos.isEmpty()) {
        e.kdos = QStandardPaths::findExecutable(QStringLiteral("kdos"), {QStringLiteral("/usr/local/bin")});
    }
    e.modelOverride = qEnvironmentVariable("KDOS_WHISPER_MODEL");
    e.dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList all = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &d : all) {
        if (d != e.dataHome) {
            e.systemDirs.append(d);
        }
    }
    return e;
}

QString kdosWhisperModel(const SpeechEnv &env)
{
    if (!env.modelOverride.isEmpty() && QFileInfo(env.modelOverride).isFile()) {
        return env.modelOverride;
    }
    QStringList dirs;
    if (!env.dataHome.isEmpty()) {
        dirs.append(env.dataHome);
    }
    dirs += env.systemDirs;
    for (const QString &d : std::as_const(dirs)) {
        const QDir models(d + QLatin1String("/whisper.cpp/models"));
        const QStringList found = models.entryList({QStringLiteral("ggml-*.bin")}, QDir::Files | QDir::Readable, QDir::Name);
        if (!found.isEmpty()) {
            return models.filePath(found.first());
        }
    }
    return {};
}

bool kdosTranscribeOffered(const SpeechEnv &env, const QStringList &mimeTypes, bool allLocal)
{
    if (!allLocal || mimeTypes.isEmpty() || env.whisperCli.isEmpty() || env.kdos.isEmpty()) {
        return false;
    }
    const bool media = std::all_of(mimeTypes.cbegin(), mimeTypes.cend(), [](const QString &m) {
        return m.startsWith(QLatin1String("audio/")) || m.startsWith(QLatin1String("video/"));
    });
    return media && !kdosWhisperModel(env).isEmpty();
}

QStringList kdosTranscribeArguments(const QStringList &localFiles)
{
    return QStringList{QStringLiteral("speech"), QStringLiteral("transcribe"), QStringLiteral("--notify"), QStringLiteral("--")} + localFiles;
}

} // namespace Kdos
