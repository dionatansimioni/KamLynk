#pragma once

#include "core/PropertyInfo.h"

#include <QDateTime>
#include <QString>
#include <QVector>
#include <cstdint>

// Pure functions translating raw Sony SDK values to/from human-readable form.
// Nothing here talks to a camera, so it is fully unit-testable.
namespace kl::codec {

// Bytes per element for an SDK CrDataType (0 for strings/unknown).
int elementSize(quint32 dataType);
bool isSigned(quint32 dataType);
bool isRange(quint32 dataType);
bool isString(quint32 dataType);

// Sign-extends a raw SDK value according to its data type.
qint64 normalize(quint32 dataType, quint64 raw);

// Decodes the packed value table returned by CrDeviceProperty::GetValues()/GetSetValues().
QVector<qint64> decodeValues(quint32 dataType, const quint8 *data, quint32 byteSize);

// Fills values/range fields of `info` from a decoded value table.
void applyValueTable(PropertyInfo &info, const QVector<qint64> &table);

QString formatFNumber(qint64 value);
QString formatShutterSpeed(qint64 value);
QString formatIso(qint64 value);
QString formatExposureBias(qint64 value);
QString formatExposureProgram(qint64 value);
QString formatWhiteBalance(qint64 value);
QString formatFocusMode(qint64 value);
QString formatFocusArea(qint64 value);
// "agora", "há 5 min", "há 3 h", "ontem", "há 4 dias", then the date.
QString formatRelativeTime(const QDateTime &when, const QDateTime &now);
QString formatDuration(qint64 seconds);

// Movie "Recording Setting": "30p · 100 Mbps · XAVC S 4K"; bitrate in Mbps (0 if unknown).
QString formatRecordingSetting(qint64 value);
int recordingSettingMbps(qint64 value);
// Movie file format with the resolution it records: "XAVC S 4K · 3840×2160".
QString formatMovieFileFormat(qint64 value);
// One or two sentences explaining what a setting does (tooltips).
QString propertyDescription(quint32 code);

// Human-readable text for any property value; falls back to the raw number.
QString format(quint32 code, qint64 value);

// Short label for a property (e.g. "ISO", "Abertura").
QString propertyName(quint32 code);

// Remote touch position: x in [0,639] in the upper 16 bits, y in [0,479] in the lower.
// nx/ny are normalized [0,1] coordinates inside the live view image (clamped).
quint32 packTouchPosition(double nx, double ny);

// User-facing explanation for an SDK error/warning code.
QString describeError(quint32 error);

// "192.168.0.10" -> SDK byte order (first octet in the lowest byte). False on bad input.
bool parseIPv4(const QString &text, quint32 *out);

} // namespace kl::codec
