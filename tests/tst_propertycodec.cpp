#include "core/PropertyCodec.h"

#include "CrDefines.h"
#include "CrDeviceProperty.h"
#include "CrError.h"

#include <QTest>

using namespace kl;
namespace SDK = SCRSDK;

class TestPropertyCodec : public QObject
{
    Q_OBJECT
private slots:
    void elementSizes()
    {
        QCOMPARE(codec::elementSize(SDK::CrDataType_UInt8Array), 1);
        QCOMPARE(codec::elementSize(SDK::CrDataType_Int16Range), 2);
        QCOMPARE(codec::elementSize(SDK::CrDataType_UInt32), 4);
        QCOMPARE(codec::elementSize(SDK::CrDataType_UInt64Array), 8);
        QCOMPARE(codec::elementSize(SDK::CrDataType_STR), 0);
        QVERIFY(codec::isSigned(SDK::CrDataType_Int8Range));
        QVERIFY(!codec::isSigned(SDK::CrDataType_UInt16Array));
        QVERIFY(codec::isRange(SDK::CrDataType_UInt32Range));
        QVERIFY(!codec::isRange(SDK::CrDataType_STR));
    }

    void normalizeSignExtends()
    {
        // 0xEC78 = -5000 (-5.0 EV), as documented for ExposureBiasCompensation.
        QCOMPARE(codec::normalize(SDK::CrDataType_Int16Array, 0xEC78), qint64(-5000));
        QCOMPARE(codec::normalize(SDK::CrDataType_UInt16Array, 0xEC78), qint64(0xEC78));
        QCOMPARE(codec::normalize(SDK::CrDataType_Int8Range, 0xFF), qint64(-1));
        QCOMPARE(codec::normalize(SDK::CrDataType_UInt32, 0xFFFFFFFFull), qint64(0xFFFFFFFF));
    }

    void decodeValues()
    {
        const qint16 raw[] = {-3000, 0, 3000};
        const auto values = codec::decodeValues(SDK::CrDataType_Int16Array, reinterpret_cast<const quint8 *>(raw),
                                                sizeof(raw));
        QCOMPARE(values, (QVector<qint64>{-3000, 0, 3000}));

        const quint32 speeds[] = {(1u << 16) | 50, (10u << 16) | 10};
        const auto s = codec::decodeValues(SDK::CrDataType_UInt32Array, reinterpret_cast<const quint8 *>(speeds),
                                           sizeof(speeds));
        QCOMPARE(s.size(), 2);
        QCOMPARE(s[0], qint64((1u << 16) | 50));

        QVERIFY(codec::decodeValues(SDK::CrDataType_UInt16Array, nullptr, 8).isEmpty());
        // Trailing partial element is ignored.
        const quint8 odd[] = {1, 0, 2};
        QCOMPARE(codec::decodeValues(SDK::CrDataType_UInt16Array, odd, 3).size(), 1);
    }

    void applyRangeTable()
    {
        PropertyInfo p;
        p.dataType = SDK::CrDataType_Int16Range;
        codec::applyValueTable(p, {-7, 7, 1});
        QVERIFY(p.isRange);
        QCOMPARE(p.rangeMin, qint64(-7));
        QCOMPARE(p.rangeMax, qint64(7));
        QCOMPARE(p.rangeStep, qint64(1));
        QVERIFY(p.values.isEmpty());

        PropertyInfo q;
        q.dataType = SDK::CrDataType_UInt16Array;
        codec::applyValueTable(q, {1, 2, 3});
        QVERIFY(!q.isRange);
        QCOMPARE(q.values.size(), 3);
    }

    void fNumber_data()
    {
        QTest::addColumn<qint64>("raw");
        QTest::addColumn<QString>("text");
        QTest::newRow("f2.8") << qint64(280) << "F2.8";
        QTest::newRow("f4") << qint64(400) << "F4";
        QTest::newRow("f22") << qint64(2200) << "F22";
        QTest::newRow("f3.5") << qint64(350) << "F3.5";
        QTest::newRow("unknown") << qint64(SDK::CrFnumber_Unknown) << "--";
    }
    void fNumber()
    {
        QFETCH(qint64, raw);
        QFETCH(QString, text);
        QCOMPARE(codec::formatFNumber(raw), text);
    }

    void shutterSpeed_data()
    {
        QTest::addColumn<qint64>("raw");
        QTest::addColumn<QString>("text");
        QTest::newRow("1/50") << qint64((1u << 16) | 50) << "1/50";
        QTest::newRow("1/4000") << qint64((1u << 16) | 4000) << "1/4000";
        QTest::newRow("1s") << qint64((10u << 16) | 10) << "1\"";
        QTest::newRow("1.3s") << qint64((13u << 16) | 10) << "1.3\"";
        QTest::newRow("30s") << qint64((300u << 16) | 10) << "30\"";
        QTest::newRow("bulb") << qint64(SDK::CrShutterSpeed_Bulb) << "Bulb";
        QTest::newRow("nothing") << qint64(SDK::CrShutterSpeed_Nothing) << "--";
    }
    void shutterSpeed()
    {
        QFETCH(qint64, raw);
        QFETCH(QString, text);
        QCOMPARE(codec::formatShutterSpeed(raw), text);
    }

    void iso()
    {
        QCOMPARE(codec::formatIso(800), QStringLiteral("ISO 800"));
        QCOMPARE(codec::formatIso(SDK::CrISO_AUTO), QStringLiteral("ISO AUTO"));
        QCOMPARE(codec::formatIso((qint64(SDK::CrISO_MultiFrameNR) << 24) | 3200), QStringLiteral("NR ISO 3200"));
    }

    void exposureBias()
    {
        QCOMPARE(codec::formatExposureBias(0), QStringLiteral("±0.0"));
        QCOMPARE(codec::formatExposureBias(700), QStringLiteral("+0.7"));
        QCOMPARE(codec::formatExposureBias(-1300), QStringLiteral("-1.3"));
        QCOMPARE(codec::formatExposureBias(-5000), QStringLiteral("-5.0"));
    }

    void enumsAndFallback()
    {
        QCOMPARE(codec::format(SDK::CrDeviceProperty_ExposureProgramMode, SDK::CrExposure_Movie_M),
                 QStringLiteral("Filme M"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_FocusMode, SDK::CrFocus_AF_C), QStringLiteral("AF-C"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_WhiteBalance, SDK::CrWhiteBalance_AWB), QStringLiteral("AWB"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_Colortemp, 5600), QStringLiteral("5600K"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_BatteryRemain, 78), QStringLiteral("78%"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_BatteryRemain, 0xFF), QStringLiteral("--"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_PictureProfile, 0), QStringLiteral("Desligado"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_PictureProfile, 8), QStringLiteral("PP8"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_CreativeLook, SDK::CrCreativeLook_FL), QStringLiteral("FL"));
        // Unknown property: plain number.
        QCOMPARE(codec::format(0xFFFF0, 42), QStringLiteral("42"));
    }

    void connectionErrorsAreActionable()
    {
        // 0x8218 and 0x8202, as reported by a ZV-E10M2 on a home router.
        QCOMPARE(quint32(SDK::CrError_Connect_SSH_GetFingerprintFailed), 0x8218u);
        QCOMPARE(quint32(SDK::CrError_Connect_ConnectIP), 0x8202u);
        QVERIFY(codec::describeError(0x8218).contains(QStringLiteral("Autent. de acesso")));
        QVERIFY(codec::describeError(0x8202).contains(QStringLiteral("Emparelhamento")));
    }

    void relativeTime()
    {
        const QDateTime now(QDate(2026, 10, 26), QTime(15, 0));
        QCOMPARE(codec::formatRelativeTime(QDateTime(), now), QString());
        QCOMPARE(codec::formatRelativeTime(now.addSecs(-20), now), QStringLiteral("agora"));
        QCOMPARE(codec::formatRelativeTime(now.addSecs(-5 * 60), now), QStringLiteral("há 5 min"));
        QCOMPARE(codec::formatRelativeTime(now.addSecs(-3 * 3600), now), QStringLiteral("há 3 h"));
        QCOMPARE(codec::formatRelativeTime(now.addDays(-1), now), QStringLiteral("ontem"));
        QCOMPARE(codec::formatRelativeTime(now.addDays(-4), now), QStringLiteral("há 4 dias"));
        QCOMPARE(codec::formatRelativeTime(now.addDays(-30), now), QStringLiteral("26/09/2026"));
    }

    void recordingSettingNames()
    {
        QCOMPARE(codec::formatRecordingSetting(SDK::CrRecordingSettingMovie_30p_100M_XAVC_S_4K),
                 QStringLiteral("30p · 100 Mbps · XAVC S 4K"));
        QCOMPARE(codec::formatRecordingSetting(SDK::CrRecordingSettingMovie_120p_50M_1280x720),
                 QStringLiteral("120p · 50 Mbps · 1280×720"));
        QCOMPARE(codec::formatRecordingSetting(SDK::CrRecordingSettingMovie_1920x1080_60p_28M),
                 QStringLiteral("60p · 1920×1080 · 28 Mbps"));
        QCOMPARE(codec::formatRecordingSetting(SDK::CrRecordingSettingMovie_600M_422_10bit),
                 QStringLiteral("600 Mbps · 4:2:2 · 10 bits"));
        QCOMPARE(codec::formatRecordingSetting(SDK::CrRecordingSettingMovie_60p_28M_PS),
                 QStringLiteral("60p · 28 Mbps · PS"));
        // The generated name table must follow the SDK enum to its last value.
        QCOMPARE(codec::formatRecordingSetting(SDK::CrRecordingSettingMovie_260M_422_10bit),
                 QStringLiteral("260 Mbps · 4:2:2 · 10 bits"));
        QCOMPARE(codec::recordingSettingMbps(SDK::CrRecordingSettingMovie_24p_60M_XAVC_S_4K), 60);
        QCOMPARE(codec::recordingSettingMbps(0), 0);
        QCOMPARE(codec::format(SDK::CrDeviceProperty_Movie_File_Format, SDK::CrFileFormatMovie_XAVC_S_HD),
                 QStringLiteral("XAVC S HD · 1920×1080"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_MediaSLOT1_Status, SDK::CrSlotStatus_NoCard),
                 QStringLiteral("Sem cartão"));
    }

    void audio()
    {
        QCOMPARE(codec::format(SDK::CrDeviceProperty_AudioRecording, SDK::CrAudioRecording_On), QStringLiteral("Ligado"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_AudioRecording, SDK::CrAudioRecording_Off),
                 QStringLiteral("Desligado"));
        QCOMPARE(codec::format(SDK::CrDeviceProperty_AudioInputMasterLevel, 26), QStringLiteral("26"));
        QCOMPARE(codec::propertyName(SDK::CrDeviceProperty_AudioRecording), QStringLiteral("Gravar áudio"));
        QCOMPARE(codec::propertyName(SDK::CrDeviceProperty_AudioInputMasterLevel), QStringLiteral("Nível do microfone"));
    }

    void duration()
    {
        QCOMPARE(codec::formatDuration(0), QStringLiteral("00:00"));
        QCOMPARE(codec::formatDuration(65), QStringLiteral("01:05"));
        QCOMPARE(codec::formatDuration(5400), QStringLiteral("1:30:00"));
        QCOMPARE(codec::formatDuration(-1), QStringLiteral("--:--"));
    }

    void touchPosition()
    {
        QCOMPARE(codec::packTouchPosition(0, 0), quint32(0));
        QCOMPARE(codec::packTouchPosition(1, 1), quint32((639u << 16) | 479u));
        QCOMPARE(codec::packTouchPosition(0.5, 0.5), quint32((320u << 16) | 240u));
        // Clamped outside the image.
        QCOMPARE(codec::packTouchPosition(-2, 9), quint32(479u));
    }

    void errors()
    {
        QVERIFY(codec::describeError(SDK::CrError_Connect_TimeOut).contains(QStringLiteral("Tempo esgotado")));
        QVERIFY(codec::describeError(SDK::CrError_Connect_SSH_UserAuthenticationFailed).contains(QStringLiteral("senha")));
        QVERIFY(codec::describeError(SDK::CrError_Adaptor_Create).contains(QStringLiteral("CrAdapter")));
        QCOMPARE(codec::describeError(SDK::CrError_Connect_Release), QStringLiteral("Erro de conexão (0x8203)"));
        QCOMPARE(codec::describeError(0x8F01), QStringLiteral("Erro do SDK (0x8f01)"));
    }

    void ipv4()
    {
        quint32 v = 0;
        QVERIFY(codec::parseIPv4(QStringLiteral("192.168.0.10"), &v));
        QCOMPARE(v, quint32(192 | (168 << 8) | (0 << 16) | (10u << 24)));
        QVERIFY(!codec::parseIPv4(QStringLiteral("192.168.0"), &v));
        QVERIFY(!codec::parseIPv4(QStringLiteral("192.168.0.256"), &v));
        QVERIFY(!codec::parseIPv4(QStringLiteral("a.b.c.d"), &v));
        QVERIFY(!codec::parseIPv4(QStringLiteral("1..2.3"), &v));
    }
};

QTEST_GUILESS_MAIN(TestPropertyCodec)
#include "tst_propertycodec.moc"
