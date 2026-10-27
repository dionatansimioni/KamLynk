#include "core/Language.h"
#include "core/PropertyCodec.h"

#include "CrDeviceProperty.h"

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace kl;

namespace {
const QLocale kBrazil(QLocale::Portuguese, QLocale::Brazil);
const QLocale kPortugal(QLocale::Portuguese, QLocale::Portugal);
const QLocale kUs(QLocale::English, QLocale::UnitedStates);
const QLocale kGermany(QLocale::German, QLocale::Germany);

QString daylightText()
{
    return codec::format(SCRSDK::CrDeviceProperty_WhiteBalance, SCRSDK::CrWhiteBalance_Daylight);
}
} // namespace

class TestLanguage : public QObject
{
    Q_OBJECT
private slots:
    void followsTheSystemByDefault()
    {
        QCOMPARE(Language::resolve({}, kBrazil), QStringLiteral("pt_BR"));
        QCOMPARE(Language::resolve({}, kPortugal), QStringLiteral("pt_BR"));
        QCOMPARE(Language::resolve({}, kUs), QStringLiteral("en"));
        // No translation for the system language: English.
        QCOMPARE(Language::resolve({}, kGermany), QStringLiteral("en"));
    }

    void choiceOverridesTheSystem()
    {
        QCOMPARE(Language::resolve(QStringLiteral("en"), kBrazil), QStringLiteral("en"));
        QCOMPARE(Language::resolve(QStringLiteral("pt_BR"), kUs), QStringLiteral("pt_BR"));
        QCOMPARE(Language::resolve(QStringLiteral("xx"), kUs), QStringLiteral("en"));
    }

    void englishSystemTranslatesTexts()
    {
        Language language(nullptr, kUs);
        QCOMPARE(language.current(), QStringLiteral("en"));
        QCOMPARE(daylightText(), QStringLiteral("Daylight"));
        QCOMPARE(codec::propertyName(SCRSDK::CrDeviceProperty_WhiteBalance), QStringLiteral("White balance"));
        QCOMPARE(QLocale().language(), QLocale::English);
    }

    void switchesAtRunTime()
    {
        Language language(nullptr, kBrazil);
        QCOMPARE(daylightText(), QStringLiteral("Luz do dia"));
        QSignalSpy changed(&language, &Language::changed);
        language.setChoice(QStringLiteral("en"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(language.current(), QStringLiteral("en"));
        QCOMPARE(daylightText(), QStringLiteral("Daylight"));
        language.setChoice({});
        QCOMPARE(language.current(), QStringLiteral("pt_BR"));
        QCOMPARE(daylightText(), QStringLiteral("Luz do dia"));
        QCOMPARE(QLocale().language(), QLocale::Portuguese);
    }

    void pluralForms()
    {
        Language language(nullptr, kUs);
        QCOMPARE(QCoreApplication::translate("kl::CameraController", "%n câmera(s) encontrada(s)", nullptr, 1),
                 QStringLiteral("1 camera found"));
        QCOMPARE(QCoreApplication::translate("kl::CameraController", "%n câmera(s) encontrada(s)", nullptr, 3),
                 QStringLiteral("3 cameras found"));
    }

    void choiceIsRemembered()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("kamlynk.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            Language language(&settings, kBrazil);
            language.setChoice(QStringLiteral("en"));
        }
        {
            QSettings settings(path, QSettings::IniFormat);
            Language language(&settings, kBrazil);
            QCOMPARE(language.choice(), QStringLiteral("en"));
            QCOMPARE(language.current(), QStringLiteral("en"));
            language.setChoice({}); // back to the system language: nothing saved
        }
        QSettings settings(path, QSettings::IniFormat);
        QVERIFY(!settings.contains(QStringLiteral("ui/language")));
    }

    void optionsListSystemFirst()
    {
        Language language(nullptr, kUs);
        const QVariantList options = language.options();
        QCOMPARE(options.size(), 3);
        QCOMPARE(options[0].toMap().value("code").toString(), QString());
        QCOMPARE(options[0].toMap().value("name").toString(), QStringLiteral("Same as the system (English)"));
        QCOMPARE(options[1].toMap().value("name").toString(), QStringLiteral("Português (Brasil)"));
        QCOMPARE(options[2].toMap().value("name").toString(), QStringLiteral("English"));
    }
};

QTEST_GUILESS_MAIN(TestLanguage)
#include "tst_language.moc"
