#include "app/KeychainStore.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <qt6keychain/keychain.h>

namespace kl {

namespace {
const QString kService = QStringLiteral("KamLynk");

QString entryKey(const QString &fingerprint)
{
    return QStringLiteral("ssh:") + fingerprint;
}
} // namespace

KeychainStore::KeychainStore(QObject *parent)
    : CredentialStore(parent)
    , m_available(QKeychain::isAvailable())
{
    qInfo() << "system keychain available:" << m_available;
}

void KeychainStore::load(const QString &key)
{
    auto *job = new QKeychain::ReadPasswordJob(kService, this);
    job->setKey(entryKey(key));
    connect(job, &QKeychain::Job::finished, this, [this, key](QKeychain::Job *done) {
        auto *read = static_cast<QKeychain::ReadPasswordJob *>(done);
        if (read->error() != QKeychain::NoError) {
            if (read->error() != QKeychain::EntryNotFound)
                qWarning().noquote() << "keychain read:" << read->errorString();
            emit loaded(key, false, {}, {});
            return;
        }
        const QJsonObject login = QJsonDocument::fromJson(read->textData().toUtf8()).object();
        emit loaded(key, true, login.value(QStringLiteral("user")).toString(),
                    login.value(QStringLiteral("password")).toString());
    });
    job->start();
}

void KeychainStore::save(const QString &key, const QString &label, const QString &user, const QString &password)
{
    auto *job = new QKeychain::WritePasswordJob(kService, this);
    job->setKey(entryKey(key));
    const QJsonObject login{{QStringLiteral("user"), user}, {QStringLiteral("password"), password},
                            {QStringLiteral("camera"), label}};
    job->setTextData(QString::fromUtf8(QJsonDocument(login).toJson(QJsonDocument::Compact)));
    connect(job, &QKeychain::Job::finished, this, [](QKeychain::Job *done) {
        if (done->error() != QKeychain::NoError)
            qWarning().noquote() << "keychain write:" << done->errorString();
        else
            qInfo() << "login saved in the system keychain";
    });
    job->start();
}

void KeychainStore::remove(const QString &key)
{
    auto *job = new QKeychain::DeletePasswordJob(kService, this);
    job->setKey(entryKey(key));
    connect(job, &QKeychain::Job::finished, this, [](QKeychain::Job *done) {
        if (done->error() != QKeychain::NoError && done->error() != QKeychain::EntryNotFound)
            qWarning().noquote() << "keychain delete:" << done->errorString();
    });
    job->start();
}

} // namespace kl
