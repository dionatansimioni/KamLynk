#include "core/FakeBackend.h"

#include "core/PropertyCodec.h"

#include "CrCommandData.h"
#include "CrDefines.h"
#include "CrDeviceProperty.h"

#include <QDir>
#include <QPainter>
#include <QtMath>

namespace SDK = SCRSDK;

namespace kl {

namespace {
const QString kFakeId = QStringLiteral("demo-zv-e10m2");

qint64 shutter(quint32 num, quint32 den) { return qint64((num << 16) | den); }
} // namespace

FakeBackend::FakeBackend(QObject *parent)
    : CameraBackend(parent)
{
    m_frameTimer.setInterval(33);
    connect(&m_frameTimer, &QTimer::timeout, this, &FakeBackend::emitFrame);
    seedProperties();
}

void FakeBackend::addProp(quint32 code, quint32 type, qint64 current, QVector<qint64> table, bool writable)
{
    PropertyInfo p;
    p.code = code;
    p.dataType = type;
    p.current = current;
    p.readable = true;
    p.writable = writable;
    codec::applyValueTable(p, table);
    m_props.insert(code, p);
}

void FakeBackend::seedProperties()
{
    using namespace SDK;
    addProp(CrDeviceProperty_FNumber, CrDataType_UInt16Array, 400,
            {280, 320, 350, 400, 450, 500, 560, 630, 710, 800, 900, 1000, 1100, 1300, 1600, 2000, 2200}, true);
    QVector<qint64> speeds;
    for (int den : {4000, 3200, 2500, 2000, 1600, 1250, 1000, 800, 640, 500, 400, 320, 250, 200, 160, 125,
                    100, 80, 60, 50, 40, 30, 25, 20, 15, 13, 10, 8, 6, 5, 4})
        speeds << shutter(1, den);
    speeds << shutter(10, 10) << shutter(20, 10);
    addProp(CrDeviceProperty_ShutterSpeed, CrDataType_UInt32Array, shutter(1, 50), speeds, true);
    QVector<qint64> isos{CrISO_AUTO};
    for (int iso : {100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600, 2000, 2500, 3200,
                    4000, 5000, 6400, 8000, 10000, 12800, 16000, 20000, 25600, 32000})
        isos << iso;
    addProp(CrDeviceProperty_IsoSensitivity, CrDataType_UInt32Array, 800, isos, true);
    addProp(CrDeviceProperty_ExposureBiasCompensation, CrDataType_Int16Array, 0,
            {-3000, -2700, -2300, -2000, -1700, -1300, -1000, -700, -300, 0, 300, 700, 1000, 1300, 1700, 2000,
             2300, 2700, 3000}, true);
    addProp(CrDeviceProperty_ExposureProgramMode, CrDataType_UInt32Array, CrExposure_Movie_M,
            {CrExposure_Movie_P, CrExposure_Movie_A, CrExposure_Movie_S, CrExposure_Movie_M, CrExposure_Movie_Auto},
            true);
    addProp(CrDeviceProperty_WhiteBalance, CrDataType_UInt16Array, CrWhiteBalance_AWB,
            {CrWhiteBalance_AWB, CrWhiteBalance_Daylight, CrWhiteBalance_Shadow, CrWhiteBalance_Cloudy,
             CrWhiteBalance_Tungsten, CrWhiteBalance_Fluorescent_WarmWhite, CrWhiteBalance_ColorTemp,
             CrWhiteBalance_Custom_1},
            true);
    addProp(CrDeviceProperty_Colortemp, CrDataType_UInt16Range, 5500, {2500, 9900, 100}, true);
    addProp(CrDeviceProperty_FocusMode, CrDataType_UInt16Array, CrFocus_AF_C,
            {CrFocus_MF, CrFocus_AF_S, CrFocus_AF_C, CrFocus_DMF}, true);
    addProp(CrDeviceProperty_FocusArea, CrDataType_UInt16Array, CrFocusArea_Wide,
            {CrFocusArea_Wide, CrFocusArea_Zone, CrFocusArea_Center, CrFocusArea_Flexible_Spot_M,
             CrFocusArea_Tracking_Wide},
            true);
    addProp(CrDeviceProperty_SubjectRecognitionAF, CrDataType_UInt8Array, CrSubjectRecognitionAF_PriorityAF,
            {CrSubjectRecognitionAF_Off, CrSubjectRecognitionAF_OnlyAF, CrSubjectRecognitionAF_PriorityAF}, true);
    addProp(CrDeviceProperty_PictureProfile, CrDataType_UInt8Array, CrPictureProfile_Off,
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, true);
    addProp(CrDeviceProperty_CreativeLook, CrDataType_UInt16Array, CrCreativeLook_ST,
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, true);
    addProp(CrDeviceProperty_CinematicVlogSetting, CrDataType_UInt8Array, CrCinematicVlogSetting_Off,
            {CrCinematicVlogSetting_Off, CrCinematicVlogSetting_On}, true);
    addProp(CrDeviceProperty_Movie_Recording_FrameRateSetting, CrDataType_UInt8Array,
            CrRecordingFrameRateSettingMovie_24p,
            {CrRecordingFrameRateSettingMovie_60p, CrRecordingFrameRateSettingMovie_30p,
             CrRecordingFrameRateSettingMovie_24p},
            true);
    addProp(CrDeviceProperty_LiveView_Image_Quality, CrDataType_UInt16Array, CrPropertyLiveViewImageQuality_High,
            {CrPropertyLiveViewImageQuality_Low, CrPropertyLiveViewImageQuality_High}, true);
    addProp(CrDeviceProperty_StillImageStoreDestination, CrDataType_UInt16Array,
            CrStillImageStoreDestination_MemoryCard,
            {CrStillImageStoreDestination_HostPC, CrStillImageStoreDestination_MemoryCard,
             CrStillImageStoreDestination_HostPCAndMemoryCard},
            true);
    addProp(CrDeviceProperty_AudioRecording, CrDataType_UInt8Array, CrAudioRecording_On,
            {CrAudioRecording_Off, CrAudioRecording_On}, true);
    addProp(CrDeviceProperty_AudioInputMasterLevel, CrDataType_UInt16Range, 26, {0, 31, 1}, true);
    addProp(CrDeviceProperty_Movie_File_Format, CrDataType_UInt16Array, CrFileFormatMovie_XAVC_S_4K,
            {CrFileFormatMovie_XAVC_S_4K, CrFileFormatMovie_XAVC_HS_4K, CrFileFormatMovie_XAVC_S_HD}, true);
    addProp(CrDeviceProperty_Movie_Recording_Setting, CrDataType_UInt16Array, CrRecordingSettingMovie_30p_100M_XAVC_S_4K,
            {CrRecordingSettingMovie_30p_100M_XAVC_S_4K, CrRecordingSettingMovie_24p_100M_XAVC_S_4K,
             CrRecordingSettingMovie_30p_60M_XAVC_S_4K, CrRecordingSettingMovie_24p_60M_XAVC_S_4K},
            true);
    addProp(CrDeviceProperty_MediaSLOT1_Status, CrDataType_UInt16Array, CrSlotStatus_OK, {}, false);
    addProp(CrDeviceProperty_MediaSLOT1_RemainingNumber, CrDataType_UInt32Range, 4321, {0, 0x7FFFFFFF, 1}, false);
    addProp(CrDeviceProperty_RecordingState, CrDataType_UInt16Range, CrMovie_Recording_State_Not_Recording, {0, 3, 1},
            false);
    addProp(CrDeviceProperty_BatteryRemain, CrDataType_Int16Range, 78, {0, 100, 1}, false);
    addProp(CrDeviceProperty_MediaSLOT1_RemainingTime, CrDataType_UInt32Range, 5400, {0, 0x7FFFFFFF, 1}, false);
    addProp(CrDeviceProperty_DeviceOverheatingState, CrDataType_UInt8Array, CrDeviceOverheatingState_NotOverheating,
            {}, false);
    addProp(CrDeviceProperty_RemoteTouchOperationEnableStatus, CrDataType_UInt8Array, CrRemoteTouchOperation_Enable,
            {}, false);
    addProp(CrDeviceProperty_RemoteTouchOperation, CrDataType_UInt32Range, 0, {0, 0xFFFFFFFF, 1}, true);
    addProp(CrDeviceProperty_NearFar, CrDataType_Int16Range, 0, {-7, 7, 1}, true);
    addProp(CrDeviceProperty_Zoom_Operation_Status, CrDataType_UInt8Array, CrZoomOperationEnableStatus_Enable, {},
            false);
    addProp(CrDeviceProperty_Zoom_Operation, CrDataType_Int8Range, 0, {-1, 1, 1}, true);
}

void FakeBackend::discover()
{
    QMetaObject::invokeMethod(this, [this] {
        if (!m_discoverable) {
            emit discovered({});
            return;
        }
        CameraDescriptor d;
        d.id = kFakeId;
        d.model = QStringLiteral("ZV-E10M2");
        d.connection = QStringLiteral("Demo");
        d.address = QStringLiteral("192.168.122.1");
        d.uid = QStringLiteral("AA:BB:CC:DD:EE:FF");
        d.needsSsh = m_requiresCredentials;
        emit discovered({d});
    }, Qt::QueuedConnection);
}

void FakeBackend::connectCamera(const QString &id, const Credentials &credentials)
{
    QMetaObject::invokeMethod(this, [this, id, credentials] {
        if (m_unresponsive)
            return;
        if (id != kFakeId) {
            emit connectionFailed(tr("Câmera não encontrada"));
            return;
        }
        ++m_connectAttempts;
        if (m_requiresCredentials && credentials.password.isEmpty()) {
            emit credentialsRequired(id, QStringLiteral("SHA256:demo-fingerprint"));
            return;
        }
        if (m_requiresCredentials && !m_requiredPassword.isEmpty() && credentials.password != m_requiredPassword) {
            m_lastCredentials = credentials;
            emit authenticationFailed(tr("Usuário ou senha SSH incorretos."));
            return;
        }
        m_lastCredentials = credentials;
        m_connected = true;
        emit connected(QStringLiteral("ZV-E10M2"));
        emit propertiesUpdated(m_props.values());
    }, Qt::QueuedConnection);
}

void FakeBackend::connectByAddress(const QString &ip, int modelIndex, const Credentials &credentials)
{
    quint32 packed = 0;
    if (!codec::parseIPv4(ip, &packed) || modelIndex < 0) {
        QMetaObject::invokeMethod(this, [this] { emit connectionFailed(tr("Endereço IP inválido")); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!m_discoverable) {
        QMetaObject::invokeMethod(this, [this] { emit connectionFailed(tr("A câmera não respondeu")); },
                                  Qt::QueuedConnection);
        return;
    }
    connectCamera(kFakeId, credentials);
}

void FakeBackend::disconnectCamera()
{
    m_frameTimer.stop();
    m_liveView = false;
    if (!m_connected)
        return;
    m_connected = false;
    QMetaObject::invokeMethod(this, [this] { emit disconnected(QString()); }, Qt::QueuedConnection);
}

void FakeBackend::dropConnection(const QString &reason)
{
    m_frameTimer.stop();
    m_connected = false;
    emit disconnected(reason);
}

void FakeBackend::update(quint32 code, qint64 value)
{
    auto it = m_props.find(code);
    if (it == m_props.end())
        return;
    it->current = value;
    const PropertyInfo info = *it;
    QMetaObject::invokeMethod(this, [this, info] { emit propertiesUpdated({info}); }, Qt::QueuedConnection);
}

void FakeBackend::setPropertyValue(quint32 code, qint64 value)
{
    update(code, value);
}

void FakeBackend::setProperty(quint32 code, qint64 value, quint32 dataType)
{
    Q_UNUSED(dataType)
    m_setCalls.append({code, value});
    const PropertyInfo p = m_props.value(code);
    if (!m_connected || !p.writable)
        return;
    // One-shot actions: the camera acts but the stored value does not change.
    if (code == SDK::CrDeviceProperty_RemoteTouchOperation || code == SDK::CrDeviceProperty_NearFar
        || code == SDK::CrDeviceProperty_Zoom_Operation)
        return;
    if (!p.isRange && !p.values.contains(value))
        return;
    update(code, value);
}

void FakeBackend::sendCommand(Command command, bool down)
{
    m_commands.append({command, down});
    if (!m_connected)
        return;
    if (command == Command::MovieRecord) {
        if (m_recordResponse == RecordResponse::Ignore)
            return;
        if (m_recordResponse == RecordResponse::Fail && down)
            update(SDK::CrDeviceProperty_RecordingState, SDK::CrMovie_Recording_State_Recording_Failed);
        else
            update(SDK::CrDeviceProperty_RecordingState,
                   down ? SDK::CrMovie_Recording_State_Recording : SDK::CrMovie_Recording_State_Not_Recording);
    } else if (command == Command::Release && !down) {
        const qint64 dest = m_props.value(SDK::CrDeviceProperty_StillImageStoreDestination).current;
        if (dest == SDK::CrStillImageStoreDestination_MemoryCard || m_downloadDir.isEmpty())
            return;
        const QString path = QDir(m_downloadDir).filePath(QStringLiteral("KL%1.JPG").arg(++m_photoNo, 5, 10, QLatin1Char('0')));
        renderFrame().save(path, "JPG");
        QMetaObject::invokeMethod(this, [this, path] { emit downloadCompleted(path); }, Qt::QueuedConnection);
    }
}

void FakeBackend::setLiveViewEnabled(bool enabled)
{
    m_liveView = enabled && m_connected;
    if (m_liveView && m_animated)
        m_frameTimer.start();
    else
        m_frameTimer.stop();
}

void FakeBackend::setDownloadDirectory(const QString &path)
{
    m_downloadDir = path;
}

void FakeBackend::emitFrame()
{
    if (!m_liveView)
        return;
    emit liveViewFrame(renderFrame());
}

QImage FakeBackend::renderFrame()
{
    // Like the camera: Low quality streams 640 px wide frames, High 1024 px.
    const bool low = m_props.value(SDK::CrDeviceProperty_LiveView_Image_Quality).current
                     == SDK::CrPropertyLiveViewImageQuality_Low;
    const int w = low ? 640 : 1024, h = low ? 360 : 576;
    QImage img(w, h, QImage::Format_RGB32);
    QPainter p(&img);
    const double t = ++m_frameNo / 30.0;
    QLinearGradient g(0, 0, w, h);
    g.setColorAt(0, QColor::fromHsvF(std::fmod(t * 0.02, 1.0), 0.55, 0.35));
    g.setColorAt(1, QColor::fromHsvF(std::fmod(t * 0.02 + 0.3, 1.0), 0.6, 0.15));
    p.fillRect(img.rect(), g);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 200, 120, 200));
    p.drawEllipse(QPointF(w / 2 + std::sin(t) * w / 3, h / 2 + std::cos(t * 1.3) * h / 4), 60, 60);
    p.setPen(QColor(255, 255, 255, 160));
    QFont f = p.font();
    f.setPixelSize(28);
    p.setFont(f);
    p.drawText(img.rect().adjusted(24, 24, -24, -24), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("KamLynk demo · ZV-E10M2"));
    return img;
}

} // namespace kl
