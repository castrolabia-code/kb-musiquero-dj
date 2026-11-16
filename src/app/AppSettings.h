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

        // --- Mixer y salidas -------------------------------------------------------------
        int  getCrossfaderCurve() const;     // 0 suave, 1 corte, 2 scratch
        void setCrossfaderCurve (int curve);
        bool getCrossfaderReverse() const;
        void setCrossfaderReverse (bool reverse);
        int  getMasterOutputPair() const;    // 0 = salidas 1-2, 1 = salidas 3-4
        void setMasterOutputPair (int pair);
        bool getSplit() const;
        void setSplit (bool split);
        /** Duración del AUTO MIX (perilla MIX), 2..60 s. */
        double getAutoMixSeconds() const;
        void   setAutoMixSeconds (double seconds);

        /** Ranuras del sampler (JSON: archivo, volumen y modo de cada una). */
        juce::String getSamplerState() const;
        void setSamplerState (const juce::String& json);

        /** Unidades de FX (JSON: efecto, beats, wet, parámetro y destino de cada una). */
        juce::String getFxState() const;
        void setFxState (const juce::String& json);

        /** Columnas de la biblioteca (orden, ancho y visibles). */
        juce::String getLibraryColumns() const;
        void setLibraryColumns (const juce::String& state);

        /** Vista "Biblioteca grande" (controlador achicado). */
        bool getLargeLibrary() const;
        void setLargeLibrary (bool on);
        /** Dibujo con la GPU (OpenGL). */
        bool getUseOpenGL() const;
        void setUseOpenGL (bool on);

        /** Clave de producto (Fase 9). */
        juce::String getLicenseKey() const;
        void setLicenseKey (const juce::String& key);
        /** Último informe de error ya mostrado (ms). */
        juce::int64 getLastCrashSeen() const;
        void setLastCrashSeen (juce::int64 ms);
        /** Atajos de teclado (JSON). */
        juce::String getKeymap() const;
        void setKeymap (const juce::String& json);

        /** Escribe a disco inmediatamente (por defecto se guarda con demora). */
        bool flush();

        juce::File getSettingsFile() const;

    private:
        juce::PropertiesFile& file() const;

        mutable juce::ApplicationProperties properties;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppSettings)
    };
} // namespace kb::app
