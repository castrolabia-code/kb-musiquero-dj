#pragma once
// =============================================================================
//  AudioDeviceService: puente entre el dispositivo de audio del sistema
//  (WASAPI compartido/exclusivo, ASIO opcional) y el motor kb::engine.
//
//  * Abre hasta 4 salidas (master 1-2 y cue 3-4, ruteo en la Fase 2).
//  * Guarda y restaura la configuración del dispositivo en AppSettings.
//  * El callback de audio solo delega en AudioEngine::process (tiempo real).
//
//  Esta clase es específica de escritorio; en Android se reemplaza por su
//  equivalente sobre Oboe sin tocar el motor.
// =============================================================================
#include <juce_audio_devices/juce_audio_devices.h>

#include "engine/AudioEngine.h"

#include <atomic>

namespace kb::app
{
    class AppSettings;

    class AudioDeviceService final : private juce::AudioIODeviceCallback,
                                     private juce::ChangeListener
    {
    public:
        static constexpr int kMaxOutputChannels = engine::AudioEngine::kMaxOutputChannels;

        AudioDeviceService (engine::AudioEngine& engine, AppSettings& settings);
        ~AudioDeviceService() override;

        /** Abre el dispositivo guardado (o el predeterminado). Devuelve el error, o vacío si salió bien. */
        juce::String initialise();

        juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

        struct Status
        {
            bool         open = false;
            juce::String typeName;
            juce::String deviceName;
            double       sampleRate      = 0.0;
            int          bufferSize      = 0;
            int          numOutputs      = 0;
            double       outputLatencyMs = 0.0;   // buffer + latencia informada por el driver
            double       cpuPercent      = 0.0;
        };

        /** Estado actual del dispositivo (hilo de mensajes). */
        Status getStatus() const;

        /** Cantidad de errores informados por el driver desde el arranque. */
        int getDeviceErrorCount() const noexcept { return deviceErrors.load (std::memory_order_relaxed); }

    private:
        // juce::AudioIODeviceCallback
        void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                               float* const* outputChannelData, int numOutputChannels,
                                               int numSamples,
                                               const juce::AudioIODeviceCallbackContext& context) override;
        void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
        void audioDeviceStopped() override;
        void audioDeviceError (const juce::String& errorMessage) override;

        // juce::ChangeListener (cambios de dispositivo -> guardar)
        void changeListenerCallback (juce::ChangeBroadcaster*) override;

        void saveState();

        engine::AudioEngine&     engine;
        AppSettings&             settings;
        juce::AudioDeviceManager deviceManager;
        std::atomic<int>         deviceErrors { 0 };
        bool                     initialised  { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioDeviceService)
    };
} // namespace kb::app
