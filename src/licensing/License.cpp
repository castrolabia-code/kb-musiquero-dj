#include "licensing/License.h"

#include "licensing/PublicKey.h"

namespace kb::licensing
{
    static constexpr const char* kPrefix = "KBDJ1-";

    static juce::BigInteger hashOf (const juce::String& payload)
    {
        const juce::SHA256 sha (payload.toUTF8());
        juce::BigInteger v;
        v.loadFromMemoryBlock (sha.getRawData());
        return v;
    }

    juce::String signLicense (const LicenseInfo& info, const juce::RSAKey& privateKey)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", info.id);
        o->setProperty ("name", info.name);
        o->setProperty ("email", info.email);
        o->setProperty ("edition", info.edition);
        o->setProperty ("issued", info.issuedMs);
        o->setProperty ("expires", info.expiresMs);
        o->setProperty ("machine", info.machine);
        const auto payload = juce::JSON::toString (juce::var (o), true);

        auto sig = hashOf (payload);
        privateKey.applyToValue (sig);
        const auto sigBlock = sig.toMemoryBlock();
        const auto data = juce::MemoryBlock (payload.toRawUTF8(), payload.getNumBytesAsUTF8());
        return kPrefix + juce::Base64::toBase64 (data.getData(), data.getSize()) + "."
               + juce::Base64::toBase64 (sigBlock.getData(), sigBlock.getSize());
    }

    Result verifyLicense (const juce::String& rawKey, const juce::RSAKey& publicKey, const juce::String& machine, juce::Time now)
    {
        Result r;
        // Se tolera que la clave venga pegada con espacios o saltos de línea.
        const auto key = rawKey.removeCharacters (" \t\r\n");
        if (key.isEmpty())
            return r;   // missing
        r.status = Status::malformed;
        if (! key.startsWith (kPrefix) || ! key.containsChar ('.'))
            return r;

        const auto body = key.substring ((int) std::strlen (kPrefix));
        juce::MemoryOutputStream payloadOut, sigOut;
        if (! juce::Base64::convertFromBase64 (payloadOut, body.upToFirstOccurrenceOf (".", false, false))
            || ! juce::Base64::convertFromBase64 (sigOut, body.fromFirstOccurrenceOf (".", false, false)))
            return r;

        const auto payload = payloadOut.toString();
        const auto json = juce::JSON::parse (payload);
        if (! json.isObject())
            return r;

        // Firma: la clave pública "deshace" la privada y tiene que dar el hash de los datos.
        juce::BigInteger sig;
        sig.loadFromMemoryBlock (sigOut.getMemoryBlock());
        if (! publicKey.isValid() || ! publicKey.applyToValue (sig) || sig != hashOf (payload))
        {
            r.status = Status::badSignature;
            return r;
        }

        r.info.id        = json.getProperty ("id", {}).toString();
        r.info.name      = json.getProperty ("name", {}).toString();
        r.info.email     = json.getProperty ("email", {}).toString();
        r.info.edition   = json.getProperty ("edition", "Pro").toString();
        r.info.machine   = json.getProperty ("machine", {}).toString();
        r.info.issuedMs  = (juce::int64) json.getProperty ("issued", 0);
        r.info.expiresMs = (juce::int64) json.getProperty ("expires", 0);

        if (r.info.expiresMs > 0 && now.toMilliseconds() > r.info.expiresMs)
            r.status = Status::expired;
        else if (r.info.machine.isNotEmpty() && r.info.machine != machine)
            r.status = Status::wrongMachine;
        else
            r.status = Status::valid;
        return r;
    }

    juce::RSAKey productPublicKey()
    {
        return juce::RSAKey (kProductPublicKey);
    }

    juce::String machineId()
    {
        // Corto y estable: hash del identificador del equipo que da el sistema.
        const juce::SHA256 sha (juce::SystemStats::getUniqueDeviceID().toUTF8());
        return sha.toHexString().substring (0, 16).toUpperCase();
    }

    juce::String statusKey (Status s)
    {
        switch (s)
        {
            case Status::valid:        return "license.status.valid";
            case Status::missing:      return "license.status.missing";
            case Status::malformed:    return "license.status.malformed";
            case Status::badSignature: return "license.status.badSignature";
            case Status::expired:      return "license.status.expired";
            case Status::wrongMachine: return "license.status.wrongMachine";
        }
        return "license.status.malformed";
    }

    juce::URL onlineValidationUrl()
    {
        // Preparado para la Fase 9+: cuando exista el servidor de licencias de Castro Labs,
        // poner acá su dirección (p. ej. https://licencias.castrolabs.../api/v1/validate).
        return {};
    }
} // namespace kb::licensing
