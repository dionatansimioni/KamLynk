#include "core/CameraBackend.h"

namespace kl {

void registerMetaTypes()
{
    // Signals cross threads (SDK worker -> UI), so the payload types must be registered.
    qRegisterMetaType<PropertyInfo>();
    qRegisterMetaType<PropertyList>();
    qRegisterMetaType<CameraDescriptor>();
    qRegisterMetaType<QList<CameraDescriptor>>();
}

} // namespace kl
