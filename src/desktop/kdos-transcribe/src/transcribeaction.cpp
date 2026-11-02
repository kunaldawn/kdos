/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdostranscribe — "Transcribe" on audio and video files
 *
 * A file-item action rather than a service menu: a service menu is listed
 * whenever its file is installed, and this one must be hidden while there is
 * no whisper-cli or no model to give it. The transcription itself is
 * `kdos speech transcribe`, which writes <file>.txt beside each file and says
 * when it is done; nothing here waits for it.
 */

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QPointer>
#include <QProcess>
#include <QWidget>

#include "transcribe.h"

namespace Kdos
{

class TranscribeAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT

public:
    TranscribeAction(QObject *parent, const QVariantList &)
        : KAbstractFileItemActionPlugin(parent)
    {
    }

    QList<QAction *> actions(const KFileItemListProperties &props, QWidget *parentWidget) override
    {
        QStringList mimes;
        QStringList files;
        bool allLocal = props.isLocal();
        for (const KFileItem &item : props.items()) {
            if (item.isDir() || item.localPath().isEmpty()) {
                allLocal = false;
                break;
            }
            mimes.append(item.mimetype());
            files.append(item.localPath());
        }
        const SpeechEnv env = kdosSpeechEnvironment();
        if (!kdosTranscribeOffered(env, mimes, allLocal)) {
            return {};
        }

        auto *action = new QAction(QIcon::fromTheme(QStringLiteral("text-x-generic-symbolic")),
                                   i18ndcp("kdos-transcribe", "@action:inmenu", "Transcribe", "Transcribe %1 Files", int(files.size())),
                                   parentWidget);
        action->setToolTip(i18ndc("kdos-transcribe", "@info:tooltip", "Write what is said into a text file beside it, on this machine"));
        const QString kdos = env.kdos;
        /* The action outlives a menu's plugin object in some hosts, so it is
         * the connection's context and the plugin is reached only if alive. */
        QPointer<TranscribeAction> self(this);
        connect(action, &QAction::triggered, action, [self, kdos, files]() {
            if (!QProcess::startDetached(kdos, kdosTranscribeArguments(files)) && self) {
                Q_EMIT self->error(i18ndc("kdos-transcribe", "@info", "Could not start the transcription."));
            }
        });
        return {action};
    }
};

} // namespace Kdos

using Kdos::TranscribeAction;
K_PLUGIN_CLASS_WITH_JSON(TranscribeAction, "kdostranscribe.json")

#include "transcribeaction.moc"
