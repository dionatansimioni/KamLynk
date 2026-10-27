#include "core/Language.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSettings>

namespace kl {

namespace {

const QString kLanguageKey = QStringLiteral("ui/language");
const QString kPortuguese = QStringLiteral("pt_BR");
const QString kEnglish = QStringLiteral("en");

QString nativeName(const QString &code)
{
    return code == kPortuguese ? QStringLiteral("Português (Brasil)") : QStringLiteral("English");
}

} // namespace

Language::Language(QSettings *settings, const QLocale &system, QObject *parent)
    : QObject(parent), m_settings(settings), m_system(system)
{
    if (m_settings) {
        const QString saved = m_settings->value(kLanguageKey).toString();
        if (available().contains(saved))
            m_choice = saved;
    }
    apply();
}

Language::~Language()
{
    if (m_installed)
        QCoreApplication::removeTranslator(&m_translator);
}

QString Language::resolve(const QString &choice, const QLocale &system)
{
    if (available().contains(choice))
        return choice;
    return system.language() == QLocale::Portuguese ? kPortuguese : kEnglish;
}

QStringList Language::available()
{
    return {kPortuguese, kEnglish};
}

void Language::setChoice(const QString &choice)
{
    const QString value = available().contains(choice) ? choice : QString();
    if (value == m_choice)
        return;
    m_choice = value;
    if (m_settings) {
        if (value.isEmpty())
            m_settings->remove(kLanguageKey);
        else
            m_settings->setValue(kLanguageKey, value);
    }
    apply();
}

QVariantList Language::options() const
{
    QVariantList out;
    out.append(QVariantMap{
        {QStringLiteral("code"), QString()},
        {QStringLiteral("name"), tr("Mesmo do sistema (%1)").arg(nativeName(resolve({}, m_system)))}});
    for (const QString &code : available())
        out.append(QVariantMap{{QStringLiteral("code"), code}, {QStringLiteral("name"), nativeName(code)}});
    return out;
}

void Language::apply()
{
    const QString language = resolve(m_choice, m_system);
    if (m_installed) {
        QCoreApplication::removeTranslator(&m_translator);
        m_installed = false;
    }
    // Portuguese is the source language: no translator needed.
    if (language != kPortuguese) {
        if (m_translator.load(QStringLiteral(":/i18n/kamlynk_%1.qm").arg(language)))
            m_installed = QCoreApplication::installTranslator(&m_translator);
        else
            qWarning().noquote() << "translation not found:" << language;
    }
    QLocale::setDefault(QLocale(language));
    m_current = language;
    emit changed();
}

} // namespace kl
