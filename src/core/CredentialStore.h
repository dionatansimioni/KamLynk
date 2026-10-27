#pragma once

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>

namespace kl {

// Where saved camera logins live. Keys are the camera's SSH host key fingerprint, so a saved
// password is only ever sent to the same camera (a different fingerprint finds nothing).
// Asynchronous because the system keychain may need to unlock or ask the user.
class CredentialStore : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    virtual bool available() const = 0;
    virtual void load(const QString &key) = 0; // answers with loaded()
    virtual void save(const QString &key, const QString &label, const QString &user, const QString &password) = 0;
    virtual void remove(const QString &key) = 0;

signals:
    void loaded(const QString &key, bool found, const QString &user, const QString &password);
};

// In-memory store, for tests and the demo mode.
class MemoryCredentialStore : public CredentialStore
{
    Q_OBJECT
public:
    using CredentialStore::CredentialStore;

    bool available() const override { return true; }
    void load(const QString &key) override;
    void save(const QString &key, const QString &label, const QString &user, const QString &password) override;
    void remove(const QString &key) override { m_items.remove(key); }

    bool contains(const QString &key) const { return m_items.contains(key); }
    QPair<QString, QString> item(const QString &key) const { return m_items.value(key); }

private:
    QHash<QString, QPair<QString, QString>> m_items;
};

} // namespace kl
