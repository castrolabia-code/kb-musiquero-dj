#pragma once
// =============================================================================
//  Licencias (Fase 9): clave de producto firmada (RSA 2048 + SHA-256).
//
//  Formato de la clave:   KBDJ1-<datos en base64>.<firma en base64>
//    datos = JSON { id, name, email, edition, issued, expires (0 = nunca), machine ("" = cualquiera) }
//    firma = RSA privada (solo la tiene Castro Labs) sobre el SHA-256 de los datos
//
//  El programa trae solo la clave PÚBLICA: puede verificar sin internet, pero
//  no puede fabricar claves. Las claves se generan con tools/license_tool.
//  La validación online (activar / revocar) queda preparada: onlineValidationUrl().
//
//  Sin licencia válida el programa no arranca (decisión del usuario: sin prueba).
// =============================================================================
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace kb::licensing
{
    struct LicenseInfo
    {
        juce::String id, name, email, edition = "Pro", machine;
        juce::int64  issuedMs = 0;
        juce::int64  expiresMs = 0;   // 0 = perpetua
    };

    enum class Status { valid, missing, malformed, badSignature, expired, wrongMachine };

    struct Result
    {
        Status status = Status::missing;
        LicenseInfo info;
        bool isValid() const noexcept { return status == Status::valid; }
    };

    /** Genera una clave firmada (herramienta de Castro Labs, nunca en el programa). */
    juce::String signLicense (const LicenseInfo& info, const juce::RSAKey& privateKey);

    /** Verifica una clave. machineId: el de esta PC (se compara si la licencia está atada a una). */
    Result verifyLicense (const juce::String& key, const juce::RSAKey& publicKey,
                          const juce::String& machineId, juce::Time now = juce::Time::getCurrentTime());

    /** Clave pública del producto (incluida en el programa). */
    juce::RSAKey productPublicKey();

    /** Identificador de esta PC (para licencias atadas a una máquina). */
    juce::String machineId();

    /** Clave de traducción del mensaje para un estado. */
    juce::String statusKey (Status s);

    /** Validación online (activar / revocar): dirección del servidor. Vacía = desactivada. */
    juce::URL onlineValidationUrl();
} // namespace kb::licensing
