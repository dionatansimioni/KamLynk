#pragma once

#include "core/CredentialStore.h"

namespace kl {

// Saved logins in the desktop keychain (KWallet, GNOME Keyring or any Secret Service) via
// QtKeychain. Nothing is written to KamLynk's own files.
class KeychainStore : public CredentialStore
{
    Q_OBJECT
public:
    explicit KeychainStore(QObject *parent = nullptr);

    bool available() const override { return m_available; }
    void load(const QString &key) override;
    void save(const QString &key, const QString &label, const QString &user, const QString &password) override;
    void remove(const QString &key) override;

private:
    bool m_available;
};

} // namespace kl
