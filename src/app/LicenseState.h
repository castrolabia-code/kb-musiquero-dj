#pragma once
// Licencia activa de esta sesión (para "Acerca de"). Solo hilo de mensajes.
#include "licensing/License.h"

namespace kb::app
{
    inline licensing::Result& currentLicenseStorage() { static licensing::Result r; return r; }
    inline void setLicense (const licensing::Result& r) { currentLicenseStorage() = r; }
    inline const licensing::Result& currentLicense() { return currentLicenseStorage(); }
} // namespace kb::app
