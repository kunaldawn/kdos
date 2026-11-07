/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   transcribe — when "Transcribe" is offered, and what it runs
 *
 * Plain functions over the environment and a list of files, so the QtTest
 * suite decides with fixture directories and no file manager.
 */

#pragma once

#include <QString>
#include <QStringList>

namespace Kdos
{

/* What deciding needs to know, read from the process by default. */
struct SpeechEnv {
    QString whisperCli;     /* whisper-cli on PATH, or empty */
    QString kdos;           /* kdos on PATH or in /usr/local/bin, or empty */
    QString modelOverride;  /* $KDOS_WHISPER_MODEL */
    QString dataHome;       /* $XDG_DATA_HOME */
    QStringList systemDirs; /* where packages install models */
};

SpeechEnv kdosSpeechEnvironment();

/* The model whisper-cli would be given: $KDOS_WHISPER_MODEL when it is a
 * file, else the first ggml-*.bin in the user's whisper.cpp/models, else in a
 * system one. Empty when there is none, which hides the action. */
QString kdosWhisperModel(const SpeechEnv &env);

/* Offered only with whisper-cli, a model and kdos all present, and only for
 * local audio or video files. */
bool kdosTranscribeOffered(const SpeechEnv &env, const QStringList &mimeTypes, bool allLocal);

/* `kdos speech transcribe --notify -- <files>`, the arguments after `kdos`. */
QStringList kdosTranscribeArguments(const QStringList &localFiles);

} // namespace Kdos
