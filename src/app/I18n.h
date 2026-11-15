#pragma once
// =============================================================================
//  I18n: idioma de la interfaz (Español / Inglés), cambiable en vivo.
//
//  * Los textos viven en assets/i18n/{es,en}.txt (embebidos en el ejecutable).
//  * En el código se usan CLAVES, nunca textos: kb::tr ("menu.options").
//  * Las traducciones también se instalan como mapeo global de JUCE, así los
//    componentes internos de JUCE (p. ej. el selector de audio) se traducen.
//  * Uso exclusivo desde el hilo de mensajes (UI).
// =============================================================================
#include <juce_core/juce_core.h>

#include <array>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

namespace kb
{
    enum class Language
    {
        Spanish,
        English
    };

    class I18n final
    {
    public:
        struct Listener
        {
            virtual ~Listener() = default;
            /** Se llama después de cambiar el idioma: refrescar todos los textos. */
            virtual void languageChanged() = 0;
        };

        static I18n& getInstance();

        void     setLanguage (Language newLanguage);
        Language getLanguage() const noexcept { return current; }

        void addListener (Listener* l);
        void removeListener (Listener* l);

        /** Texto crudo del archivo de traducción embebido. */
        static juce::String loadTranslationText (Language language);

        /** "es" / "en" */
        static juce::String          toCode (Language language);
        static std::optional<Language> fromCode (const juce::String& code);

        /** Idioma del sistema operativo: inglés si el SO está en inglés; si no, español. */
        static Language detectSystemLanguage();

        /** Idiomas disponibles, en el orden en que se muestran. */
        static constexpr std::array<Language, 2> allLanguages() { return { Language::Spanish, Language::English }; }

        /** Libera el mapeo global (llamar al cerrar la app). */
        static void shutdown();

    private:
        I18n() = default;

        Language current { Language::Spanish };
        bool     installed { false };
        std::vector<Listener*> listeners;
    };

    /** Traduce una clave con el idioma actual. Si falta, devuelve la clave (y avisa en Debug). */
    juce::String tr (const juce::String& key);

    /** Traduce y reemplaza variables: tr ("about.body", { { "version", "0.1.0" } }) */
    juce::String tr (const juce::String& key,
                     std::initializer_list<std::pair<const char*, juce::String>> args);
} // namespace kb
