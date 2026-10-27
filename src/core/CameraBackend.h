#pragma once

#include "core/PropertyInfo.h"

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>

namespace kl {

struct CameraDescriptor
{
    QString id;          // opaque key used to connect
    QString model;       // e.g. "ZV-E10M2"
    QString connection;  // "Wi-Fi/LAN" or "USB"
    QString address;     // IP or USB serial, for display
    QString uid;         // stable identity: Wi-Fi MAC or USB serial (empty for manual IP)
    bool needsSsh = false;
};

void registerMetaTypes();

struct Credentials
{
    QString user;
    QString password;
    QString fingerprint;
};

// Abstract camera transport. SonyBackend talks to real cameras through the Sony SDK,
// FakeBackend simulates one (tests and --demo mode). All methods are non-blocking:
// results come back through signals, possibly from another thread.
class CameraBackend : public QObject
{
    Q_OBJECT
public:
    enum class Command { Release, MovieRecord, CancelRemoteTouch };
    Q_ENUM(Command)

    using QObject::QObject;
    ~CameraBackend() override = default;

    virtual void discover() = 0;
    virtual void connectCamera(const QString &id, const Credentials &credentials) = 0;
    // modelIndex is an SCRSDK::CrCameraDeviceModelList value.
    virtual void connectByAddress(const QString &ip, int modelIndex, const Credentials &credentials) = 0;
    virtual void disconnectCamera() = 0;

    virtual void setProperty(quint32 code, qint64 value, quint32 dataType) = 0;
    virtual void sendCommand(Command command, bool down) = 0;
    virtual void setLiveViewEnabled(bool enabled) = 0;
    virtual void setDownloadDirectory(const QString &path) = 0;

signals:
    void discovered(const QList<kl::CameraDescriptor> &cameras);
    void discoveryFailed(const QString &reason);
    void credentialsRequired(const QString &id, const QString &fingerprint);
    void connected(const QString &modelName);
    void connectionFailed(const QString &reason);
    // Wrong SSH user/password: the connection can be retried with new credentials.
    void authenticationFailed(const QString &reason);
    void disconnected(const QString &reason);
    void reconnecting();
    void reconnected();
    void propertiesUpdated(const kl::PropertyList &properties);
    void liveViewFrame(const QImage &frame);
    void downloadCompleted(const QString &path);
    void warning(const QString &message);
};

} // namespace kl

Q_DECLARE_METATYPE(kl::CameraDescriptor)
