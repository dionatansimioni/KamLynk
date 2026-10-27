#include "app/AppSetup.h"
#include "app/KeychainStore.h"
#include "core/CameraController.h"
#include "core/DesktopIntegration.h"
#include "core/FakeBackend.h"
#include "core/Language.h"
#include "sdk/SonyBackend.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

Q_IMPORT_QML_PLUGIN(KamLynkPlugin)

namespace {

// Translation context of the command-line help.
struct Cli
{
    Q_DECLARE_TR_FUNCTIONS(CommandLine)
};

QFile *g_logFile = nullptr;
QtMessageHandler g_previousHandler = nullptr;

// Mirrors every log line to ~/.local/share/KamLynk/kamlynk.log for bug reports.
void logToFile(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    static QMutex mutex;
    {
        QMutexLocker lock(&mutex);
        if (g_logFile) {
            static const char *levels[] = {"DEBUG", "WARN", "CRIT", "FATAL", "INFO"};
            const QString line = QStringLiteral("%1 [%2] %3\n")
                                     .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                          QLatin1String(levels[type]), message);
            g_logFile->write(line.toUtf8());
            g_logFile->flush();
        }
    }
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}

void setupLogging()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    g_logFile = new QFile(QDir(dir).filePath(QStringLiteral("kamlynk.log")));
    if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        delete g_logFile;
        g_logFile = nullptr;
    }
    g_previousHandler = qInstallMessageHandler(logToFile);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("KamLynk"));
    QGuiApplication::setApplicationVersion(QStringLiteral(KAMLYNK_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("kamlynk"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/packaging/kamlynk.svg")));

    // Before anything shows text (also --help). Shared by demo mode.
    QSettings languageSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("KamLynk"),
                               QStringLiteral("kamlynk"));
    kl::Language language(&languageSettings);

    QCommandLineParser parser;
    parser.setApplicationDescription(Cli::tr("Controle remoto para câmeras Sony"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption demo(QStringLiteral("demo"), Cli::tr("Usa uma câmera simulada (sem hardware)."));
    parser.addOption(demo);
    QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                  Cli::tr("Salva uma captura da janela e sai (testes visuais)."),
                                  Cli::tr("arquivo.png"));
    parser.addOption(screenshot);
    QCommandLineOption sdkCheck(QStringLiteral("sdk-check"),
                                Cli::tr("Testa o carregamento do SDK da Sony e sai."));
    parser.addOption(sdkCheck);
    QCommandLineOption sshCheck(QStringLiteral("ssh-check"),
                                Cli::tr("Lê a impressão digital SSH de uma câmera (diagnóstico) e sai."),
                                QStringLiteral("ip"));
    parser.addOption(sshCheck);
    parser.process(app);

    if (parser.isSet(sdkCheck)) {
        QString report;
        const bool ok = kl::SonyBackend::selfCheck(&report);
        fprintf(stdout, "%s\n", qPrintable(report));
        return ok ? 0 : 1;
    }

    if (parser.isSet(sshCheck)) {
        QString report;
        const int result = kl::SonyBackend::sshCheck(parser.value(sshCheck), &report);
        fprintf(stdout, "%s\n", qPrintable(report));
        return result;
    }

    setupLogging();

    // Running as AppImage: register the .desktop entry + icon so KDE/GNOME (Wayland) show the
    // app icon and list KamLynk in the menu. Skipped for screenshots/demo runs.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty() && !parser.isSet(screenshot)) {
        QFile icon(QStringLiteral(":/packaging/kamlynk.svg"));
        QString error;
        const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        const QByteArray svg = icon.open(QIODevice::ReadOnly) ? icon.readAll() : QByteArray();
        if (kl::desktop::install(appImage, dataHome, svg, &error))
            qInfo().noquote() << "desktop entry installed for" << appImage;
        else if (!error.isEmpty())
            qWarning().noquote() << "desktop entry:" << error;
    }
    QQuickStyle::setStyle(QStringLiteral("Material"));

    kl::CameraBackend *backend = nullptr;
    if (parser.isSet(demo)) {
        auto *fake = new kl::FakeBackend;
        fake->setAnimated(true);
        backend = fake;
    } else {
        backend = new kl::SonyBackend;
    }
    // Demo mode keeps its preferences apart from the real camera's.
    auto *settings = parser.isSet(demo)
                         ? new QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("KamLynk"),
                                         QStringLiteral("kamlynk-demo"))
                         : new QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("KamLynk"),
                                         QStringLiteral("kamlynk"));
    // Demo mode never touches the real keychain.
    kl::CredentialStore *credentials = parser.isSet(demo) ? static_cast<kl::CredentialStore *>(new kl::MemoryCredentialStore)
                                                          : new kl::KeychainStore;
    kl::CameraController controller(backend, settings, credentials);
    // Screenshots skip the first-start terms without saving an acceptance.
    if (parser.isSet(screenshot))
        controller.setTermsAccepted(true, false);
    kl::registerQmlTypes(&controller, &language);

    QQmlApplicationEngine engine;
    kl::retranslateOnLanguageChange(&language, &engine, &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("KamLynk", "Main");

    if (parser.isSet(screenshot)) {
        controller.discover();
        // In demo mode, connect to the simulated camera so the shooting screen is captured.
        if (parser.isSet(demo)) {
            QObject::connect(&controller, &kl::CameraController::camerasChanged, &controller,
                             [&controller] { controller.connectTo(0); }, Qt::SingleShotConnection);
        }
        const QString path = parser.value(screenshot);
        QTimer::singleShot(2500, &app, [&engine, path] {
            auto *window = engine.rootObjects().isEmpty()
                               ? nullptr
                               : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            const bool saved = window && window->grabWindow().save(path);
            QCoreApplication::exit(saved ? 0 : 1);
        });
    } else if (controller.termsAccepted()) {
        // Reconnect to last session's camera; otherwise just fill the camera list.
        controller.reconnectLast();
    } else {
        // First start: talk to cameras only after the terms were accepted.
        QObject::connect(&controller, &kl::CameraController::termsAcceptedChanged, &controller,
                         [&controller] { controller.reconnectLast(); }, Qt::SingleShotConnection);
    }
    return app.exec();
}
