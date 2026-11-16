// Pruebas de la Fase 9: licencias firmadas.
#include <catch2/catch_test_macros.hpp>

#include "licensing/License.h"

using namespace kb::licensing;

namespace
{
    struct Keys
    {
        Keys() { juce::RSAKey::createKeyPair (pub, priv, 1024); }   // 1024 en pruebas: más rápido
        juce::RSAKey pub, priv;
    };
    const Keys& keys() { static Keys k; return k; }

    LicenseInfo customer()
    {
        LicenseInfo i;
        i.id = "TEST01";
        i.name = juce::String::fromUTF8 ("José Pérez");
        i.email = "cliente@example.com";
        i.issuedMs = juce::Time::currentTimeMillis();
        return i;
    }
}

TEST_CASE ("Licencia: una clave firmada se valida sin internet", "[license]")
{
    const auto key = signLicense (customer(), keys().priv);
    CHECK (key.startsWith ("KBDJ1-"));
    const auto r = verifyLicense (key, keys().pub, "PC1");
    REQUIRE (r.isValid());
    CHECK (r.info.name == juce::String::fromUTF8 ("José Pérez"));
    CHECK (r.info.email == "cliente@example.com");

    // Pegada con espacios y saltos de línea (como llega en un mail): igual vale.
    CHECK (verifyLicense ("  " + key.substring (0, 40) + "\n" + key.substring (40) + " ", keys().pub, "PC1").isValid());
}

TEST_CASE ("Licencia: datos cambiados, otra firma o basura no valen", "[license]")
{
    const auto key = signLicense (customer(), keys().priv);

    // Cambiar el nombre (re-codificando los datos) rompe la firma.
    auto info = customer();
    info.name = "Otro";
    const auto otherPayload = signLicense (info, keys().priv);
    const auto forged = otherPayload.upToFirstOccurrenceOf (".", true, false) + key.fromFirstOccurrenceOf (".", false, false);
    CHECK (verifyLicense (forged, keys().pub, "PC1").status == Status::badSignature);

    // Firmada con otra clave privada.
    juce::RSAKey pub2, priv2;
    juce::RSAKey::createKeyPair (pub2, priv2, 1024);
    CHECK (verifyLicense (signLicense (customer(), priv2), keys().pub, "PC1").status == Status::badSignature);

    CHECK (verifyLicense ("", keys().pub, "PC1").status == Status::missing);
    CHECK (verifyLicense ("hola", keys().pub, "PC1").status == Status::malformed);
    CHECK (verifyLicense ("KBDJ1-@@@.###", keys().pub, "PC1").status == Status::malformed);
}

TEST_CASE ("Licencia: vencimiento y atada a una PC", "[license]")
{
    auto info = customer();
    info.expiresMs = juce::Time::currentTimeMillis() - 1000;
    CHECK (verifyLicense (signLicense (info, keys().priv), keys().pub, "PC1").status == Status::expired);

    info.expiresMs = juce::Time::currentTimeMillis() + 86400000;
    CHECK (verifyLicense (signLicense (info, keys().priv), keys().pub, "PC1").isValid());

    info.expiresMs = 0;
    info.machine = "PC1";
    const auto bound = signLicense (info, keys().priv);
    CHECK (verifyLicense (bound, keys().pub, "PC1").isValid());
    CHECK (verifyLicense (bound, keys().pub, "PC2").status == Status::wrongMachine);
}

TEST_CASE ("Licencia: la clave publica del producto esta incluida y es valida", "[license]")
{
    CHECK (productPublicKey().isValid());
    CHECK (machineId().length() == 16);
    CHECK (machineId() == machineId());   // estable
}

#include "app/Keymap.h"

TEST_CASE ("Teclado: atajos predeterminados, reasignar y guardar", "[keys]")
{
    kb::app::Keymap k;
    CHECK (k.targetFor (juce::KeyPress ('q')) == "deck1.playPause");
    CHECK (k.targetFor (juce::KeyPress ('Q')) == "deck1.playPause");   // con Bloq Mayús también
    CHECK (k.targetFor (juce::KeyPress ('a')) == "deck1.cue");
    CHECK (k.targetFor (juce::KeyPress ('s')) == "deck2.cue");
    CHECK (k.targetFor (juce::KeyPress ('b', juce::ModifierKeys::commandModifier, 0)) == "library.largeView");
    CHECK (k.targetFor (juce::KeyPress ('b')).isEmpty());

    k.assign (juce::KeyPress ('p'), "deck1.cue");         // CUE pasa a la P (la A queda libre)
    CHECK (k.targetFor (juce::KeyPress ('p')) == "deck1.cue");
    CHECK (k.targetFor (juce::KeyPress ('a')).isEmpty());
    k.assign (juce::KeyPress ('p'), "deck2.sync");        // la P ahora hace otra cosa
    CHECK (k.targetFor (juce::KeyPress ('p')) == "deck2.sync");
    CHECK_FALSE (k.keyFor ("deck1.cue").isValid());

    kb::app::Keymap loaded;
    REQUIRE (loaded.fromJson (k.toJson()));
    CHECK (loaded.targetFor (juce::KeyPress ('p')) == "deck2.sync");
    CHECK (loaded.entries().size() == k.entries().size());
    loaded.resetToDefaults();
    CHECK (loaded.targetFor (juce::KeyPress ('a')) == "deck1.cue");
}
