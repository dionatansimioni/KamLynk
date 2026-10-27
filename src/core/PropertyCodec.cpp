#include "core/PropertyCodec.h"

#include "CrDefines.h"
#include "CrDeviceProperty.h"
#include "CrError.h"

#include <QCoreApplication>
#include <QStringList>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <cstring>

namespace SDK = SCRSDK;

namespace kl::codec {

namespace {

// Translation context for the texts below (Text::tr).
struct Text
{
    Q_DECLARE_TR_FUNCTIONS(PropertyCodec)
};

constexpr quint32 kBaseMask = 0x0F;

QString trimmedDecimal(double value, int decimals)
{
    QString s = QString::number(value, 'f', decimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    return s;
}

} // namespace

int elementSize(quint32 dataType)
{
    if (dataType == SDK::CrDataType_STR)
        return 0;
    switch (dataType & kBaseMask) {
    case SDK::CrDataType_UInt8: return 1;
    case SDK::CrDataType_UInt16: return 2;
    case SDK::CrDataType_UInt32: return 4;
    case SDK::CrDataType_UInt64: return 8;
    default: return 0; // 128-bit and undefined are not supported
    }
}

bool isSigned(quint32 dataType)
{
    return dataType != SDK::CrDataType_STR && (dataType & SDK::CrDataType_SignBit);
}

bool isRange(quint32 dataType)
{
    return dataType != SDK::CrDataType_STR && (dataType & SDK::CrDataType_RangeBit);
}

bool isString(quint32 dataType)
{
    return dataType == SDK::CrDataType_STR;
}

qint64 normalize(quint32 dataType, quint64 raw)
{
    const int size = elementSize(dataType);
    if (size == 0 || size == 8)
        return static_cast<qint64>(raw);
    const int bits = size * 8;
    const quint64 mask = (quint64(1) << bits) - 1;
    raw &= mask;
    if (isSigned(dataType) && (raw & (quint64(1) << (bits - 1))))
        return static_cast<qint64>(raw | ~mask);
    return static_cast<qint64>(raw);
}

QVector<qint64> decodeValues(quint32 dataType, const quint8 *data, quint32 byteSize)
{
    QVector<qint64> out;
    const int size = elementSize(dataType);
    if (!data || size == 0)
        return out;
    const quint32 count = byteSize / size;
    out.reserve(count);
    for (quint32 i = 0; i < count; ++i) {
        quint64 raw = 0;
        std::memcpy(&raw, data + i * size, size); // little-endian host, as the SDK
        out.append(normalize(dataType, raw));
    }
    return out;
}

void applyValueTable(PropertyInfo &info, const QVector<qint64> &table)
{
    info.isRange = isRange(info.dataType);
    if (info.isRange) {
        info.values.clear();
        if (table.size() >= 3) {
            info.rangeMin = table[0];
            info.rangeMax = table[1];
            info.rangeStep = table[2] > 0 ? table[2] : 1;
        }
    } else {
        info.values = table;
    }
}

QString formatFNumber(qint64 value)
{
    if (value == 0 || value == SDK::CrFnumber_Unknown || value == SDK::CrFnumber_Nothing)
        return QStringLiteral("--");
    if (value == SDK::CrFnumber_IrisClose)
        return Text::tr("Fechada");
    return QStringLiteral("F") + trimmedDecimal(value / 100.0, 1);
}

QString formatShutterSpeed(qint64 value)
{
    const quint32 v = static_cast<quint32>(value);
    if (v == SDK::CrShutterSpeed_Bulb)
        return QStringLiteral("Bulb");
    if (v == SDK::CrShutterSpeed_Nothing)
        return QStringLiteral("--");
    const quint32 num = (v >> 16) & 0xFFFF;
    const quint32 den = v & 0xFFFF;
    if (den == 0)
        return QStringLiteral("--");
    if (num == 1)
        return QStringLiteral("1/%1").arg(den);
    return trimmedDecimal(double(num) / double(den), 1) + QLatin1Char('"');
}

QString formatIso(qint64 value)
{
    const quint32 v = static_cast<quint32>(value);
    const quint32 mode = (v >> 24) & 0x0F;
    const quint32 iso = v & 0x00FFFFFF;
    QString prefix;
    if (mode == SDK::CrISO_MultiFrameNR)
        prefix = QStringLiteral("NR ");
    else if (mode == SDK::CrISO_MultiFrameNR_High)
        prefix = QStringLiteral("NR+ ");
    if (iso == SDK::CrISO_AUTO)
        return prefix + QStringLiteral("ISO AUTO");
    return prefix + QStringLiteral("ISO %1").arg(iso);
}

QString formatExposureBias(qint64 value)
{
    if (value == 0)
        return QStringLiteral("±0.0");
    const double ev = value / 1000.0;
    return (ev > 0 ? QStringLiteral("+") : QString()) + QString::number(ev, 'f', 1);
}

QString formatExposureProgram(qint64 value)
{
    switch (static_cast<quint32>(value)) {
    case SDK::CrExposure_M_Manual: return QStringLiteral("M");
    case SDK::CrExposure_P_Auto: return QStringLiteral("P");
    case SDK::CrExposure_A_AperturePriority: return QStringLiteral("A");
    case SDK::CrExposure_S_ShutterSpeedPriority: return QStringLiteral("S");
    case SDK::CrExposure_Auto: return QStringLiteral("Auto");
    case SDK::CrExposure_Auto_Plus: return QStringLiteral("Auto+");
    case SDK::CrExposure_Movie_P: return Text::tr("Filme P");
    case SDK::CrExposure_Movie_A: return Text::tr("Filme A");
    case SDK::CrExposure_Movie_S: return Text::tr("Filme S");
    case SDK::CrExposure_Movie_M: return Text::tr("Filme M");
    case SDK::CrExposure_Movie_Auto: return Text::tr("Filme Auto");
    case SDK::CrExposure_Movie_F: return Text::tr("Filme Flex");
    case SDK::CrExposure_Movie_SQMotion_P: return QStringLiteral("S&Q P");
    case SDK::CrExposure_Movie_SQMotion_A: return QStringLiteral("S&Q A");
    case SDK::CrExposure_Movie_SQMotion_S: return QStringLiteral("S&Q S");
    case SDK::CrExposure_Movie_SQMotion_M: return QStringLiteral("S&Q M");
    case SDK::CrExposure_Movie_SQMotion_AUTO: return QStringLiteral("S&Q Auto");
    case SDK::CrExposure_HiFrameRate_P: return QStringLiteral("HFR P");
    case SDK::CrExposure_HiFrameRate_A: return QStringLiteral("HFR A");
    case SDK::CrExposure_HiFrameRate_S: return QStringLiteral("HFR S");
    case SDK::CrExposure_HiFrameRate_M: return QStringLiteral("HFR M");
    case SDK::CrExposure_SQMotion_P: return QStringLiteral("S&Q P");
    case SDK::CrExposure_SQMotion_A: return QStringLiteral("S&Q A");
    case SDK::CrExposure_SQMotion_S: return QStringLiteral("S&Q S");
    case SDK::CrExposure_SQMotion_M: return QStringLiteral("S&Q M");
    case SDK::CrExposure_MOVIE: return Text::tr("Filme");
    case SDK::CrExposure_STILL: return Text::tr("Foto");
    case SDK::CrExposure_Portrait: return Text::tr("Retrato");
    case SDK::CrExposure_Sports_Action: return Text::tr("Esportes");
    case SDK::CrExposure_Night: return Text::tr("Noite");
    case SDK::CrExposure_Landscape: return Text::tr("Paisagem");
    case SDK::CrExposure_Macro: return QStringLiteral("Macro");
    case SDK::CrExposure_Flash_Off: return Text::tr("Sem flash");
    default: return Text::tr("Modo 0x%1").arg(value, 0, 16);
    }
}

QString formatWhiteBalance(qint64 value)
{
    switch (static_cast<quint16>(value)) {
    case SDK::CrWhiteBalance_AWB: return QStringLiteral("AWB");
    case SDK::CrWhiteBalance_Underwater_Auto: return Text::tr("Subaquático");
    case SDK::CrWhiteBalance_Daylight: return Text::tr("Luz do dia");
    case SDK::CrWhiteBalance_Shadow: return Text::tr("Sombra");
    case SDK::CrWhiteBalance_Cloudy: return Text::tr("Nublado");
    case SDK::CrWhiteBalance_Tungsten: return Text::tr("Incandescente");
    case SDK::CrWhiteBalance_Fluorescent: return Text::tr("Fluorescente");
    case SDK::CrWhiteBalance_Fluorescent_WarmWhite: return Text::tr("Fluor. quente");
    case SDK::CrWhiteBalance_Fluorescent_CoolWhite: return Text::tr("Fluor. fria");
    case SDK::CrWhiteBalance_Fluorescent_DayWhite: return Text::tr("Fluor. dia branco");
    case SDK::CrWhiteBalance_Fluorescent_Daylight: return Text::tr("Fluor. luz do dia");
    case SDK::CrWhiteBalance_Flush: return QStringLiteral("Flash");
    case SDK::CrWhiteBalance_ColorTemp: return Text::tr("Temp. cor");
    case SDK::CrWhiteBalance_Custom_1: return Text::tr("Pers. 1");
    case SDK::CrWhiteBalance_Custom_2: return Text::tr("Pers. 2");
    case SDK::CrWhiteBalance_Custom_3: return Text::tr("Pers. 3");
    case SDK::CrWhiteBalance_Custom: return Text::tr("Personalizado");
    default: return QStringLiteral("WB 0x%1").arg(value, 0, 16);
    }
}

QString formatFocusMode(qint64 value)
{
    switch (static_cast<quint16>(value)) {
    case SDK::CrFocus_MF: return QStringLiteral("MF");
    case SDK::CrFocus_AF_S: return QStringLiteral("AF-S");
    case SDK::CrFocus_AF_C: return QStringLiteral("AF-C");
    case SDK::CrFocus_AF_A: return QStringLiteral("AF-A");
    case SDK::CrFocus_AF_D: return QStringLiteral("AF-D");
    case SDK::CrFocus_DMF: return QStringLiteral("DMF");
    case SDK::CrFocus_PF: return QStringLiteral("PF");
    default: return Text::tr("Foco 0x%1").arg(value, 0, 16);
    }
}

QString formatFocusArea(qint64 value)
{
    switch (static_cast<quint16>(value)) {
    case SDK::CrFocusArea_Wide: return Text::tr("Ampla");
    case SDK::CrFocusArea_Zone: return Text::tr("Zona");
    case SDK::CrFocusArea_Center: return Text::tr("Centro fixo");
    case SDK::CrFocusArea_Flexible_Spot_S: return Text::tr("Ponto S");
    case SDK::CrFocusArea_Flexible_Spot_M: return Text::tr("Ponto M");
    case SDK::CrFocusArea_Flexible_Spot_L: return Text::tr("Ponto L");
    case SDK::CrFocusArea_Expand_Flexible_Spot: return Text::tr("Ponto expandido");
    case SDK::CrFocusArea_Flexible_Spot: return Text::tr("Ponto flexível");
    case SDK::CrFocusArea_Tracking_Wide: return Text::tr("Rastreio: Ampla");
    case SDK::CrFocusArea_Tracking_Zone: return Text::tr("Rastreio: Zona");
    case SDK::CrFocusArea_Tracking_Center: return Text::tr("Rastreio: Centro");
    case SDK::CrFocusArea_Tracking_Flexible_Spot_S: return Text::tr("Rastreio: Ponto S");
    case SDK::CrFocusArea_Tracking_Flexible_Spot_M: return Text::tr("Rastreio: Ponto M");
    case SDK::CrFocusArea_Tracking_Flexible_Spot_L: return Text::tr("Rastreio: Ponto L");
    case SDK::CrFocusArea_Tracking_Expand_Flexible_Spot: return Text::tr("Rastreio: Ponto exp.");
    case SDK::CrFocusArea_Tracking_Flexible_Spot: return Text::tr("Rastreio: Ponto flex.");
    default: return Text::tr("Área 0x%1").arg(value, 0, 16);
    }
}

QString formatDuration(qint64 seconds)
{
    if (seconds < 0)
        return QStringLiteral("--:--");
    const qint64 h = seconds / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

namespace {

// CrRecordingSettingMovie names in enum order (values are sequential from 0), generated
// from CrDeviceProperty.h; turned into readable text by formatRecordingSetting().
const char *const kRecordingSettingNames[] = {
    "Invalid",
    "60p_50M",
    "30p_50M",
    "24p_50M",
    "50p_50M",
    "25p_50M",
    "60i_24M",
    "50i_24M_FX",
    "60i_17M_FH",
    "50i_17M_FH",
    "60p_28M_PS",
    "50p_28M_PS",
    "24p_24M_FX",
    "25p_24M_FX",
    "24p_17M_FH",
    "25p_17M_FH",
    "120p_50M_1280x720",
    "100p_50M_1280x720",
    "1920x1080_30p_16M",
    "1920x1080_25p_16M",
    "1280x720_30p_6M",
    "1280x720_25p_6M",
    "1920x1080_60p_28M",
    "1920x1080_50p_28M",
    "60p_25M_XAVC_S_HD",
    "50p_25M_XAVC_S_HD",
    "30p_16M_XAVC_S_HD",
    "25p_16M_XAVC_S_HD",
    "120p_100M_1920x1080_XAVC_S_HD",
    "100p_100M_1920x1080_XAVC_S_HD",
    "120p_60M_1920x1080_XAVC_S_HD",
    "100p_60M_1920x1080_XAVC_S_HD",
    "30p_100M_XAVC_S_4K",
    "25p_100M_XAVC_S_4K",
    "24p_100M_XAVC_S_4K",
    "30p_60M_XAVC_S_4K",
    "25p_60M_XAVC_S_4K",
    "24p_60M_XAVC_S_4K",
    "600M_422_10bit",
    "500M_422_10bit",
    "400M_420_10bit",
    "300M_422_10bit",
    "280M_422_10bit",
    "250M_422_10bit",
    "240M_422_10bit",
    "222M_422_10bit",
    "200M_422_10bit",
    "200M_420_10bit",
    "200M_420_8bit",
    "185M_422_10bit",
    "150M_420_10bit",
    "150M_420_8bit",
    "140M_422_10bit",
    "111M_422_10bit",
    "100M_422_10bit",
    "100M_420_10bit",
    "100M_420_8bit",
    "93M_422_10bit",
    "89M_422_10bit",
    "75M_420_10bit",
    "60M_420_8bit",
    "50M_422_10bit",
    "50M_420_10bit",
    "50M_420_8bit",
    "45M_420_10bit",
    "30M_420_10bit",
    "25M_420_8bit",
    "16M_420_8bit",
    "520M_422_10bit",
    "260M_422_10bit"
};

} // namespace

QString formatRecordingSetting(qint64 value)
{
    if (value <= 0 || value >= qint64(std::size(kRecordingSettingNames)))
        return QString::number(value);
    const QStringList tokens = QString::fromLatin1(kRecordingSettingNames[value]).split(QLatin1Char('_'));
    QStringList parts;
    QStringList codec;
    for (const QString &t : tokens) {
        if (t.endsWith(QLatin1Char('p')) || t.endsWith(QLatin1Char('i')))
            parts.prepend(t); // frame rate first
        else if (t.endsWith(QLatin1Char('M')) && t.chopped(1).toInt() > 0)
            parts << QStringLiteral("%1 Mbps").arg(t.chopped(1));
        else if (t.contains(QLatin1Char('x')) && t.split(QLatin1Char('x')).size() == 2)
            parts << QString(t).replace(QLatin1Char('x'), QChar(0x00D7));
        else if (t == QLatin1String("422") || t == QLatin1String("420"))
            parts << QStringLiteral("4:%1:%2").arg(t.at(1)).arg(t.at(2));
        else if (t.endsWith(QLatin1String("bit")))
            parts << QStringLiteral("%1 bits").arg(t.chopped(3));
        else
            codec << t; // XAVC S HD, AVCHD FX/FH/PS...
    }
    if (!codec.isEmpty())
        parts << codec.join(QLatin1Char(' '));
    return parts.join(QStringLiteral(" · "));
}

int recordingSettingMbps(qint64 value)
{
    if (value <= 0 || value >= qint64(std::size(kRecordingSettingNames)))
        return 0;
    for (const QString &t : QString::fromLatin1(kRecordingSettingNames[value]).split(QLatin1Char('_')))
        if (t.endsWith(QLatin1Char('M')) && t.chopped(1).toInt() > 0)
            return t.chopped(1).toInt();
    return 0;
}

QString formatMovieFileFormat(qint64 value)
{
    static const QString k4K = QStringLiteral("3840×2160"), kHD = QStringLiteral("1920×1080");
    switch (value) {
    case SDK::CrFileFormatMovie_AVCHD: return QStringLiteral("AVCHD · ") + kHD;
    case SDK::CrFileFormatMovie_MP4: return QStringLiteral("MP4");
    case SDK::CrFileFormatMovie_XAVC_S_4K: return QStringLiteral("XAVC S 4K · ") + k4K;
    case SDK::CrFileFormatMovie_XAVC_S_HD: return QStringLiteral("XAVC S HD · ") + kHD;
    case SDK::CrFileFormatMovie_XAVC_HS_8K: return QStringLiteral("XAVC HS 8K · 7680×4320");
    case SDK::CrFileFormatMovie_XAVC_HS_4K: return QStringLiteral("XAVC HS 4K · ") + k4K;
    case SDK::CrFileFormatMovie_XAVC_HS_HD: return QStringLiteral("XAVC HS HD · ") + kHD;
    case SDK::CrFileFormatMovie_XAVC_S_L_4K: return QStringLiteral("XAVC S-L 4K · ") + k4K;
    case SDK::CrFileFormatMovie_XAVC_S_L_HD: return QStringLiteral("XAVC S-L HD · ") + kHD;
    case SDK::CrFileFormatMovie_XAVC_S_I_4K: return QStringLiteral("XAVC S-I 4K · ") + k4K;
    case SDK::CrFileFormatMovie_XAVC_S_I_HD: return QStringLiteral("XAVC S-I HD · ") + kHD;
    case SDK::CrFileFormatMovie_XAVC_S_I_DCI_4K: return QStringLiteral("XAVC S-I DCI 4K · 4096×2160");
    default: return QString::number(value);
    }
}

QString propertyDescription(quint32 code)
{
    switch (code) {
    case SDK::CrDeviceProperty_ExposureProgramMode:
        return Text::tr("Quem controla a exposição: P (automático com ajustes), A (você escolhe a abertura), "
                        "S (você escolhe o obturador) ou M (tudo manual).");
    case SDK::CrDeviceProperty_FNumber:
        return Text::tr("Abertura do diafragma. Números menores (F1.8) deixam entrar mais luz e desfocam mais "
                        "o fundo; maiores (F8) deixam mais coisa em foco.");
    case SDK::CrDeviceProperty_ShutterSpeed:
        return Text::tr("Tempo de exposição de cada quadro. Em vídeo, use cerca do dobro da taxa de quadros "
                        "(1/50 a 25p, 1/60 a 30p) para um movimento natural.");
    case SDK::CrDeviceProperty_IsoSensitivity:
        return Text::tr("Sensibilidade do sensor. ISO alto clareia cenas escuras, mas aumenta o ruído. "
                        "AUTO deixa a câmera escolher.");
    case SDK::CrDeviceProperty_ExposureBiasCompensation:
        return Text::tr("Deixa a imagem mais clara (+) ou mais escura (−) que o medido pela câmera. "
                        "Funciona nos modos automáticos e com ISO AUTO.");
    case SDK::CrDeviceProperty_FocusMode:
        return Text::tr("AF-C acompanha o assunto continuamente (ideal para vídeo); AF-S trava o foco; "
                        "DMF permite ajuste manual depois do AF; MF é totalmente manual.");
    case SDK::CrDeviceProperty_FocusArea:
        return Text::tr("Onde a câmera procura foco: Ampla (todo o quadro), Zona, Centro, Ponto flexível ou "
                        "Rastreamento.");
    case SDK::CrDeviceProperty_SubjectRecognitionAF:
        return Text::tr("Reconhece rostos e olhos (pessoas, animais…) para focar neles automaticamente.");
    case SDK::CrDeviceProperty_WhiteBalance:
        return Text::tr("Corrige a cor da luz para que o branco fique branco. AWB é automático; os demais "
                        "fixam a cor para luz do dia, sombra, tungstênio etc.");
    case SDK::CrDeviceProperty_Colortemp:
        return Text::tr("Temperatura de cor em Kelvin, quando o balanço de branco está em Temp. cor. Valores "
                        "baixos esfriam (azul) e altos esquentam (laranja) a imagem.");
    case SDK::CrDeviceProperty_CreativeLook:
        return Text::tr("Estilos de imagem da Sony: ST (padrão), PT (retrato), VV (vívido), FL (filme), "
                        "BW (preto e branco) e outros.");
    case SDK::CrDeviceProperty_PictureProfile:
        return Text::tr("Curvas de cor e contraste para vídeo (PP1–PP11), como S-Log3 e HLG para correção "
                        "de cor na edição.");
    case SDK::CrDeviceProperty_CinematicVlogSetting:
        return Text::tr("Visual de cinema da Sony: proporção mais larga, 24p e cores com aparência de filme.");
    case SDK::CrDeviceProperty_Movie_File_Format:
        return Text::tr("Formato e resolução do vídeo gravado: 4K (3840×2160) tem mais detalhes; HD "
                        "(1920×1080) gera arquivos menores e permite taxas de quadros mais altas.");
    case SDK::CrDeviceProperty_Movie_Recording_Setting:
        return Text::tr("Taxa de quadros e qualidade (Mbps) da gravação. Mais Mbps = mais qualidade e "
                        "arquivos maiores.");
    case SDK::CrDeviceProperty_Movie_Recording_FrameRateSetting:
        return Text::tr("Quadros por segundo do vídeo: 24p tem aparência de cinema, 30p é o padrão de "
                        "internet e 60p deixa o movimento mais fluido.");
    case SDK::CrDeviceProperty_AudioRecording:
        return Text::tr("Grava ou não o som junto com o vídeo.");
    case SDK::CrDeviceProperty_AudioInputMasterLevel:
        return Text::tr("Volume de gravação do microfone. Ajuste para que a voz fique alta sem estourar. "
                        "Confira o medidor na tela da câmera.");
    case SDK::CrDeviceProperty_LiveView_Image_Quality:
        return Text::tr("Qualidade da imagem enviada para esta prévia (não afeta a gravação). Baixa é mais "
                        "fluida em Wi-Fi fraco.");
    case SDK::CrDeviceProperty_StillImageStoreDestination:
        return Text::tr("Onde as fotos tiradas pelo app são salvas: no cartão, no PC ou nos dois.");
    default:
        return {};
    }
}

QString format(quint32 code, qint64 value)
{
    switch (code) {
    case SDK::CrDeviceProperty_FNumber: return formatFNumber(value);
    case SDK::CrDeviceProperty_ShutterSpeed: return formatShutterSpeed(value);
    case SDK::CrDeviceProperty_IsoSensitivity: return formatIso(value);
    case SDK::CrDeviceProperty_ExposureBiasCompensation: return formatExposureBias(value);
    case SDK::CrDeviceProperty_ExposureProgramMode: return formatExposureProgram(value);
    case SDK::CrDeviceProperty_WhiteBalance: return formatWhiteBalance(value);
    case SDK::CrDeviceProperty_Colortemp: return QStringLiteral("%1K").arg(value);
    case SDK::CrDeviceProperty_FocusMode: return formatFocusMode(value);
    case SDK::CrDeviceProperty_FocusArea: return formatFocusArea(value);
    case SDK::CrDeviceProperty_BatteryRemain:
        return (value < 0 || value > 100) ? QStringLiteral("--") : QStringLiteral("%1%").arg(value);
    case SDK::CrDeviceProperty_MediaSLOT1_RemainingTime: return formatDuration(value);
    case SDK::CrDeviceProperty_PictureProfile:
        if (value == SDK::CrPictureProfile_Off)
            return Text::tr("Desligado");
        if (value >= SDK::CrPictureProfile_LUT_Number1)
            return QStringLiteral("LUT %1").arg(value - SDK::CrPictureProfile_LUT_Number1 + 1);
        return QStringLiteral("PP%1").arg(value);
    case SDK::CrDeviceProperty_CreativeLook: {
        static const QStringList looks = {
            QStringLiteral("ST"), QStringLiteral("PT"), QStringLiteral("NT"), QStringLiteral("VV"),
            QStringLiteral("VV2"), QStringLiteral("FL"), QStringLiteral("IN"), QStringLiteral("SH"),
            QStringLiteral("BW"), QStringLiteral("SE"), QStringLiteral("FL2"), QStringLiteral("FL3")};
        if (value >= SDK::CrCreativeLook_CustomLookOffset)
            return Text::tr("Look pers. %1").arg(value - SDK::CrCreativeLook_CustomLookOffset);
        if (value >= 1 && value <= looks.size())
            return looks.at(int(value) - 1);
        break;
    }
    case SDK::CrDeviceProperty_LiveView_Image_Quality:
        return value == SDK::CrPropertyLiveViewImageQuality_High ? Text::tr("Alta") : Text::tr("Baixa");
    case SDK::CrDeviceProperty_StillImageStoreDestination:
        switch (value) {
        case SDK::CrStillImageStoreDestination_HostPC: return Text::tr("Só no PC");
        case SDK::CrStillImageStoreDestination_MemoryCard: return Text::tr("Só no cartão");
        case SDK::CrStillImageStoreDestination_HostPCAndMemoryCard: return Text::tr("PC + cartão");
        default: break;
        }
        break;
    case SDK::CrDeviceProperty_SubjectRecognitionAF:
        switch (value) {
        case SDK::CrSubjectRecognitionAF_Off: return Text::tr("Desligado");
        case SDK::CrSubjectRecognitionAF_OnlyAF: return Text::tr("Só AF");
        case SDK::CrSubjectRecognitionAF_PriorityAF: return Text::tr("Prioridade");
        default: break;
        }
        break;
    case SDK::CrDeviceProperty_CinematicVlogSetting:
        return value == SDK::CrCinematicVlogSetting_On ? Text::tr("Ligado") : Text::tr("Desligado");
    case SDK::CrDeviceProperty_AudioRecording:
        return value == SDK::CrAudioRecording_On ? Text::tr("Ligado") : Text::tr("Desligado");
    case SDK::CrDeviceProperty_Movie_Recording_Setting: return formatRecordingSetting(value);
    case SDK::CrDeviceProperty_Movie_File_Format: return formatMovieFileFormat(value);
    case SDK::CrDeviceProperty_MediaSLOT1_Status:
        switch (value) {
        case SDK::CrSlotStatus_OK: return QStringLiteral("OK");
        case SDK::CrSlotStatus_NoCard: return Text::tr("Sem cartão");
        case SDK::CrSlotStatus_CardRecognizing: return Text::tr("Lendo o cartão…");
        case SDK::CrSlotStatus_CardError_ReadOnlyMedia: return Text::tr("Cartão protegido contra gravação");
        case SDK::CrSlotStatus_DBError_CantRepairAndNeedFormat: return Text::tr("Cartão precisa ser formatado");
        default: return Text::tr("Erro no cartão");
        }
    case SDK::CrDeviceProperty_Movie_Recording_FrameRateSetting:
        switch (value) {
        case SDK::CrRecordingFrameRateSettingMovie_120p: return QStringLiteral("120p");
        case SDK::CrRecordingFrameRateSettingMovie_100p: return QStringLiteral("100p");
        case SDK::CrRecordingFrameRateSettingMovie_60p: return QStringLiteral("60p");
        case SDK::CrRecordingFrameRateSettingMovie_50p: return QStringLiteral("50p");
        case SDK::CrRecordingFrameRateSettingMovie_30p: return QStringLiteral("30p");
        case SDK::CrRecordingFrameRateSettingMovie_25p: return QStringLiteral("25p");
        case SDK::CrRecordingFrameRateSettingMovie_24p: return QStringLiteral("24p");
        case SDK::CrRecordingFrameRateSettingMovie_23_98p: return QStringLiteral("23.98p");
        case SDK::CrRecordingFrameRateSettingMovie_29_97p: return QStringLiteral("29.97p");
        case SDK::CrRecordingFrameRateSettingMovie_59_94p: return QStringLiteral("59.94p");
        case SDK::CrRecordingFrameRateSettingMovie_24_00p: return QStringLiteral("24.00p");
        case SDK::CrRecordingFrameRateSettingMovie_119_88p: return QStringLiteral("119.88p");
        default: break;
        }
        break;
    default:
        break;
    }
    return QString::number(value);
}

QString propertyName(quint32 code)
{
    switch (code) {
    case SDK::CrDeviceProperty_FNumber: return Text::tr("Abertura");
    case SDK::CrDeviceProperty_ShutterSpeed: return Text::tr("Obturador");
    case SDK::CrDeviceProperty_IsoSensitivity: return QStringLiteral("ISO");
    case SDK::CrDeviceProperty_ExposureBiasCompensation: return Text::tr("Exposição");
    case SDK::CrDeviceProperty_ExposureProgramMode: return Text::tr("Modo");
    case SDK::CrDeviceProperty_WhiteBalance: return Text::tr("Balanço de branco");
    case SDK::CrDeviceProperty_Colortemp: return Text::tr("Temperatura");
    case SDK::CrDeviceProperty_FocusMode: return Text::tr("Modo de foco");
    case SDK::CrDeviceProperty_FocusArea: return Text::tr("Área de foco");
    case SDK::CrDeviceProperty_PictureProfile: return QStringLiteral("Picture Profile");
    case SDK::CrDeviceProperty_CreativeLook: return QStringLiteral("Creative Look");
    case SDK::CrDeviceProperty_LiveView_Image_Quality: return Text::tr("Qualidade da prévia");
    case SDK::CrDeviceProperty_StillImageStoreDestination: return Text::tr("Salvar fotos em");
    case SDK::CrDeviceProperty_SubjectRecognitionAF: return Text::tr("Reconhecimento");
    case SDK::CrDeviceProperty_CinematicVlogSetting: return QStringLiteral("Cinematic Vlog");
    case SDK::CrDeviceProperty_Movie_Recording_FrameRateSetting: return Text::tr("Taxa de quadros");
    case SDK::CrDeviceProperty_AudioRecording: return Text::tr("Gravar áudio");
    case SDK::CrDeviceProperty_AudioInputMasterLevel: return Text::tr("Nível do microfone");
    case SDK::CrDeviceProperty_Movie_File_Format: return Text::tr("Formato e resolução");
    case SDK::CrDeviceProperty_Movie_Recording_Setting: return Text::tr("Qualidade de gravação");
    default: return QStringLiteral("0x%1").arg(code, 0, 16);
    }
}

quint32 packTouchPosition(double nx, double ny)
{
    const auto clamp01 = [](double v) { return std::isnan(v) ? 0.0 : std::clamp(v, 0.0, 1.0); };
    const auto x = static_cast<quint32>(std::lround(clamp01(nx) * 639.0));
    const auto y = static_cast<quint32>(std::lround(clamp01(ny) * 479.0));
    return (x << 16) | y;
}

QString describeError(quint32 error)
{
    switch (error) {
    case SDK::CrError_None: return QStringLiteral("OK");
    case SDK::CrError_Connect_TimeOut:
        return Text::tr("Tempo esgotado. A câmera está no modo de controle remoto e na mesma rede?");
    case SDK::CrError_Connect_FailRejected:
        return Text::tr("A câmera recusou a conexão. Confirme o pareamento na tela da câmera.");
    case SDK::CrError_Connect_FailBusy: return Text::tr("A câmera está ocupada com outro dispositivo.");
    case SDK::CrError_Connect_SessionAlreadyOpened: return Text::tr("Já existe uma sessão aberta com a câmera.");
    case SDK::CrError_Connect_Disconnected: return Text::tr("A conexão com a câmera caiu.");
    case SDK::CrError_Connect_SSH_ServerAuthenticationFailed:
    case SDK::CrError_Connect_SSH_UserAuthenticationFailed:
        return Text::tr("Usuário ou senha SSH incorretos.");
    case SDK::CrError_Connect_SSH_GetFingerprintFailed:
        return Text::tr(
            "A câmera exige autenticação SSH (Autent. de acesso ligada), mas não respondeu. Desligue e "
            "ligue a câmera e tente de novo, ou desligue Rede › Opção de rede › Config. autent. acesso › "
            "Autent. de acesso e conecte por Emparelhamento.");
    case SDK::CrError_Connect_ConnectIP:
        return Text::tr(
            "A câmera não aceitou a conexão. Confira o IP e se o Disparo remoto está ligado. Com Autent. "
            "de acesso desligada, inicie o Emparelhamento na câmera antes de conectar.");
    case SDK::CrError_Adaptor_DeviceBusy: return Text::tr("Câmera ocupada, tente novamente.");
    case SDK::CrError_Adaptor_Create:
        return Text::tr("Plugins de conexão do SDK (CrAdapter) não encontrados ao lado do executável.");
    case SDK::CrError_Api_Insufficient: return Text::tr("Recurso não suportado por esta câmera.");
    default:
        break;
    }
    if ((error & 0xFF00) == SDK::CrError_Connect)
        return Text::tr("Erro de conexão (0x%1)").arg(error, 0, 16);
    return Text::tr("Erro do SDK (0x%1)").arg(error, 0, 16);
}

QString formatRelativeTime(const QDateTime &when, const QDateTime &now)
{
    if (!when.isValid())
        return {};
    const qint64 secs = when.secsTo(now);
    if (secs < 60)
        return Text::tr("agora");
    if (secs < 3600)
        return Text::tr("há %1 min").arg(secs / 60);
    const qint64 days = when.date().daysTo(now.date());
    if (days == 0)
        return Text::tr("há %1 h").arg(secs / 3600);
    if (days == 1)
        return Text::tr("ontem");
    if (days < 7)
        return Text::tr("há %1 dias").arg(days);
    return when.date().toString(Text::tr("dd/MM/yyyy"));
}

bool parseIPv4(const QString &text, quint32 *out)
{
    const QStringList parts = text.trimmed().split(QLatin1Char('.'));
    if (parts.size() != 4)
        return false;
    quint32 value = 0;
    for (int i = 0; i < 4; ++i) {
        bool ok = false;
        const uint octet = parts[i].toUInt(&ok);
        if (!ok || octet > 255 || parts[i].isEmpty())
            return false;
        value |= octet << (i * 8);
    }
    if (out)
        *out = value;
    return true;
}

} // namespace kl::codec
