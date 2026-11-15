#pragma once
// =============================================================================
//  AppSettings: ajustes persistentes del usuario.
//
//  Windows: %APPDATA%\Castro Labs\KB Musiquero DJ\settings.xml
//  (logs en la misma carpeta, subcarpeta "logs").
//  Uso desde el hilo de mensajes.
// =============================================================================
#include <juce_data_structures/juce_data_structures.h>

#include "app/I18n.h"

#include <memory>
#include <optional>

namespace kb::app
{
    class AppSettings final
    {
    public:
        AppSettings();
        ~AppSettings();

        /** Carpeta de datos de la app (se crea si no existe). */
        static juce::File getAppDataDirectory();

        std::optional<Language> getLanguage() const;
        void setLanguage (Language language);

        juce::String getMainWindowState() const;
        void setMainWindowState (const juce::String& state);

        std::unique_ptr<juce::XmlElement> getAudioDeviceState() const;
        void setAudioDeviceState (const juce::XmlElement* state);

        /** Escribe a disco inmediatamente (por defecto se guarda con demora). */
        bool flush();

        juce::File getSettingsFile() const;

    private:
        juce::PropertiesFile& file() const;

        mutable juce::ApplicationProperties properties;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppSettings)
    };
} // namespace kb::app
