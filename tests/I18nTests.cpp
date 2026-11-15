// Pruebas de internacionalización: los dos idiomas deben estar completos y
// toda clave usada en el código debe existir en los archivos de traducción.
#include <catch2/catch_test_macros.hpp>

#include "app/I18n.h"

#include <set>

using kb::I18n;
using kb::Language;

namespace
{
    std::set<juce::String> keysOf (Language lang)
    {
        juce::LocalisedStrings ls (I18n::loadTranslationText (lang), false);
        std::set<juce::String> keys;
        for (const auto& k : ls.getMappings().getAllKeys())
            keys.insert (k);
        return keys;
    }

    juce::String describe (const std::set<juce::String>& s)
    {
        juce::StringArray a;
        for (const auto& k : s) a.add (k);
        return a.joinIntoString (", ");
    }
}

TEST_CASE ("I18n: español e inglés tienen exactamente las mismas claves", "[i18n]")
{
    const auto es = keysOf (Language::Spanish);
    const auto en = keysOf (Language::English);

    REQUIRE (es.size() > 50);

    std::set<juce::String> missingInEn, missingInEs;
    for (const auto& k : es) if (! en.count (k)) missingInEn.insert (k);
    for (const auto& k : en) if (! es.count (k)) missingInEs.insert (k);

    INFO ("Faltan en en.txt: " << describe (missingInEn));
    INFO ("Faltan en es.txt: " << describe (missingInEs));
    CHECK (missingInEn.empty());
    CHECK (missingInEs.empty());
}

TEST_CASE ("I18n: los archivos declaran su idioma", "[i18n]")
{
    CHECK (juce::LocalisedStrings (I18n::loadTranslationText (Language::Spanish), false).getLanguageName() == juce::String::fromUTF8 ("Español"));
    CHECK (juce::LocalisedStrings (I18n::loadTranslationText (Language::English), false).getLanguageName() == "English");
}

TEST_CASE ("I18n: tr() traduce y cambia de idioma en vivo", "[i18n]")
{
    struct Probe final : I18n::Listener
    {
        int calls = 0;
        void languageChanged() override { ++calls; }
    } probe;

    auto& i18n = I18n::getInstance();
    i18n.addListener (&probe);

    i18n.setLanguage (Language::Spanish);
    CHECK (kb::tr ("menu.options") == "Opciones");

    i18n.setLanguage (Language::English);
    CHECK (kb::tr ("menu.options") == "Options");
    CHECK (i18n.getLanguage() == Language::English);
    CHECK (probe.calls >= 1);

    // Las cadenas internas de JUCE (TRANS) también se traducen.
    i18n.setLanguage (Language::Spanish);
    CHECK (TRANS ("Sample rate:") == "Frecuencia de muestreo:");

    // Variables
    CHECK (kb::tr ("options.audio.latency", { { "latency", "5.3" } }) == "Latencia de salida estimada: 5.3 ms");

    // Clave inexistente: devuelve la clave
    CHECK (kb::tr ("no.existe") == "no.existe");

    i18n.removeListener (&probe);
}

TEST_CASE ("I18n: códigos de idioma", "[i18n]")
{
    CHECK (I18n::fromCode ("es") == Language::Spanish);
    CHECK (I18n::fromCode ("es-AR") == Language::Spanish);
    CHECK (I18n::fromCode ("EN") == Language::English);
    CHECK_FALSE (I18n::fromCode ("pt").has_value());
    CHECK (I18n::toCode (Language::English) == "en");
}

TEST_CASE ("I18n: toda clave usada con tr(\"...\") en el código existe", "[i18n]")
{
    const auto keys = keysOf (Language::Spanish);
    const juce::File src = juce::File (KB_SOURCE_DIR).getChildFile ("src");
    REQUIRE (src.isDirectory());

    std::set<juce::String> missing;
    for (const auto& entry : juce::RangedDirectoryIterator (src, true, "*.cpp;*.h"))
    {
        const auto text = entry.getFile().loadFileAsString();
        // Busca  tr ("clave"  y  tr("clave"  (las claves armadas con "+" se ignoran)
        for (const char* pattern : { " tr (\"", " tr(\"", "(tr (\"", "(tr(\"" })
        {
            for (int pos = text.indexOf (pattern); pos >= 0; pos = text.indexOf (pos + 1, pattern))
            {
                const auto start = text.indexOfChar (pos + 1, '"') + 1;
                const auto end   = text.indexOfChar (start, '"');
                const auto key   = text.substring (start, end);
                if (key.containsChar ('.') && ! key.endsWithChar ('.') && ! keys.count (key))
                    missing.insert (key + "  (" + entry.getFile().getFileName() + ")");
            }
        }
    }

    INFO ("Claves usadas en el código sin traducción: " << describe (missing));
    CHECK (missing.empty());
}
