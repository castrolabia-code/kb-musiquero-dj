#include "app/AudioDeviceService.h"

#include "app/AppSettings.h"

namespace kb::app
{
    AudioDeviceService::AudioDeviceService (engine::AudioEngine& e, AppSettings& s)
        : engine (e), settings (s)
    {
    }

    AudioDeviceService::~AudioDeviceService()
    {
        if (initialised)
        {
            deviceManager.removeChangeListener (this);
            saveState();
            deviceManager.removeAudioCallback (this);
            deviceManager.closeAudioDevice();
        }
    }

    juce::String AudioDeviceService::initialise()
    {
        jassert (! initialised);

        const auto saved = settings.getAudioDeviceState();

        // Sin entradas por ahora (micrófono/timecode llegarían en fases futuras).
        // Si el dispositivo guardado no está (p. ej. placa desconectada), JUCE
        // abre el predeterminado en lugar de fallar.
        const auto error = deviceManager.initialise (0, kMaxOutputChannels, saved.get(), true);

        deviceManager.addAudioCallback (this);
        deviceManager.addChangeListener (this);
        initialised = true;

        if (error.isNotEmpty())
            juce::Logger::writeToLog ("Audio: error al abrir el dispositivo: " + error);

        return error;
    }

    AudioDeviceService::Status AudioDeviceService::getStatus() const
    {
        Status st;
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr || ! device->isOpen())
            return st;

        st.open       = true;
        st.typeName   = device->getTypeName();
        st.deviceName = device->getName();
        st.sampleRate = device->getCurrentSampleRate();
        st.bufferSize = device->getCurrentBufferSizeSamples();
        st.numOutputs = device->getActiveOutputChannels().countNumberOfSetBits();
        st.cpuPercent = deviceManager.getCpuUsage() * 100.0;

        if (st.sampleRate > 0.0)
            st.outputLatencyMs = 1000.0 * (st.bufferSize + device->getOutputLatencyInSamples()) / st.sampleRate;

        return st;
    }

    // --- Hilo de audio ---------------------------------------------------------------
    void AudioDeviceService::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                               float* const* outputChannelData, int numOutputChannels,
                                                               int numSamples,
                                                               const juce::AudioIODeviceCallbackContext&)
    {
        // Solo delega en el motor. Nada de allocations, locks, I/O ni logs acá.
        engine.process (outputChannelData, numOutputChannels, numSamples);
    }

    void AudioDeviceService::audioDeviceAboutToStart (juce::AudioIODevice* device)
    {
        // Llamado con el audio detenido: es seguro reservar memoria en el motor.
        engine::EngineConfig config;
        config.sampleRate        = device->getCurrentSampleRate();
        config.maxBlockSize      = device->getCurrentBufferSizeSamples();
        config.numOutputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
        engine.prepare (config);
    }

    void AudioDeviceService::audioDeviceStopped()
    {
        engine.release();
    }

    void AudioDeviceService::audioDeviceError (const juce::String&)
    {
        // Puede llegar desde el hilo de audio: solo contamos (sin logs ni allocations).
        deviceErrors.fetch_add (1, std::memory_order_relaxed);
    }

    // --- Hilo de mensajes -------------------------------------------------------------
    void AudioDeviceService::changeListenerCallback (juce::ChangeBroadcaster*)
    {
        saveState();

        const auto st = getStatus();
        if (st.open)
            juce::Logger::writeToLog ("Audio: " + st.typeName + " / " + st.deviceName + " / "
                                      + juce::String (st.sampleRate, 0) + " Hz / "
                                      + juce::String (st.bufferSize) + " muestras / "
                                      + juce::String (st.numOutputs) + " salidas");
    }

    void AudioDeviceService::saveState()
    {
        const auto state = deviceManager.createStateXml();
        settings.setAudioDeviceState (state.get());
    }
} // namespace kb::app
