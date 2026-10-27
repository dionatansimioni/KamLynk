#pragma once

#include <QLocale>
#include <QObject>
#include <QTranslator>
#include <QVariantList>

class QSettings;

namespace kl {

// UI language. Source texts are Brazilian Portuguese; other languages come from the
// translations embedded at :/i18n. The default follows the system language (Portuguese
// systems get Portuguese, everything else English) until the user picks one, which is saved.
class Language : public QObject
{
    Q_OBJECT
    // "" follows the system; otherwise one of the codes in `options`.
    Q_PROPERTY(QString choice READ choice WRITE setChoice NOTIFY changed)
    // Language in use: "pt_BR" or "en".
    Q_PROPERTY(QString current READ current NOTIFY changed)
    // [{code, name}] for the settings picker, "follow the system" first.
    Q_PROPERTY(QVariantList options READ options NOTIFY changed)

public:
    // Does not take ownership of `settings` (may be null: nothing is saved).
    explicit Language(QSettings *settings = nullptr, const QLocale &system = QLocale::system(),
                      QObject *parent = nullptr);
    ~Language() override;

    // Language used for `choice` on a system with locale `system`.
    static QString resolve(const QString &choice, const QLocale &system);
    static QStringList available();

    QString choice() const { return m_choice; }
    void setChoice(const QString &choice);
    QString current() const { return m_current; }
    QVariantList options() const;

signals:
    // After the translator was swapped: retranslate the UI.
    void changed();

private:
    void apply();

    QSettings *m_settings;
    QLocale m_system;
    QString m_choice;
    QString m_current;
    QTranslator m_translator;
    bool m_installed = false;
};

} // namespace kl
