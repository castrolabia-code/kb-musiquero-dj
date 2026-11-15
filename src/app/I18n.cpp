#include "app/I18n.h"

#include <KbBinaryData.h>

#include <algorithm>

namespace kb
{
    I18n& I18n::getInstance()
    {
        static I18n instance;
        return instance;
    }

    juce::String I18n::loadTranslationText (Language language)
    {
        switch (language)
        {
            case Language::English: return juce::String::createStringFromData (KbBinaryData::en_txt, KbBinaryData::en_txtSize);
            case Language::Spanish: break;
        }
        return juce::String::createStringFromData (KbBinaryData::es_txt, KbBinaryData::es_txtSize);
    }

    juce::String I18n::toCode (Language language)
    {
        return language == Language::English ? "en" : "es";
    }

    std::optional<Language> I18n::fromCode (const juce::String& code)
    {
        const auto c = code.trim().toLowerCase();
        if (c.startsWith ("es")) return Language::Spanish;
        if (c.startsWith ("en")) return Language::English;
        return std::nullopt;
    }

    Language I18n::detectSystemLanguage()
    {
        // Producto pensado para hispanohablantes: cualquier idioma que no sea
        // inglés arranca en español.
        return fromCode (juce::SystemStats::getUserLanguage()).value_or (Language::Spanish);
    }

    void I18n::setLanguage (Language newLanguage)
    {
        if (installed && newLanguage == current)
            return;

        current   = newLanguage;
        installed = true;

        // JUCE toma posesión del objeto.
        juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (loadTranslationText (current), false));

        // Copia: un listener puede quitarse a sí mismo durante la notificación.
        const auto toNotify = listeners;
        for (auto* l : toNotify)
            if (std::find (listeners.begin(), listeners.end(), l) != listeners.end())
                l->languageChanged();
    }

    void I18n::addListener (Listener* l)
    {
        jassert (l != nullptr);
        if (std::find (listeners.begin(), listeners.end(), l) == listeners.end())
            listeners.push_back (l);
    }

    void I18n::removeListener (Listener* l)
    {
        listeners.erase (std::remove (listeners.begin(), listeners.end(), l), listeners.end());
    }

    void I18n::shutdown()
    {
        juce::LocalisedStrings::setCurrentMappings (nullptr);
        getInstance().installed = false;
    }

    juce::String tr (const juce::String& key)
    {
        if (auto* mappings = juce::LocalisedStrings::getCurrentMappings())
        {
            const auto missing = juce::String::charToString (0x1);
            const auto text    = mappings->translate (key, missing);
            if (text != missing)
                return text;
        }

        // Clave sin traducir: en Debug avisa; en Release muestra la clave
        // (preferible a un texto vacío).
        DBG ("kb::tr - falta la clave de traduccion: " << key);
        return key;
    }

    juce::String tr (const juce::String& key,
                     std::initializer_list<std::pair<const char*, juce::String>> args)
    {
        auto text = tr (key);
        for (const auto& [name, value] : args)
            text = text.replace ("{" + juce::String (name) + "}", value);
        return text;
    }
} // namespace kb
