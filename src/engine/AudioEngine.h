#pragma once
// =============================================================================
//  AudioEngine: raíz del motor de audio de KB - MUSIQUERO DJ.
//
//  No conoce el dispositivo de audio: recibe buffers de salida y los llena.
//  La capa de dispositivo (app/AudioDeviceService en Windows, Oboe en Android)
//  solo llama a prepare() / process() / release().
//
//  Fase 0: el motor está listo y genera silencio. En la Fase 1 se agregan los
//  decks, el mixer y el ruteo, siempre dentro de process().
//
//  Hilos:
//    * prepare() / release(): hilo de control, con el audio DETENIDO.
//    * process(): hilo de audio. Sin allocations, locks, I/O ni logs.
//    * getters: cualquier hilo (atómicos).
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/RealtimeChecks.h"

#include <atomic>
#include <cstdint>

namespace kb::engine
{
    struct EngineConfig
    {
        double sampleRate        = 48000.0;
        int    maxBlockSize      = 512;
        int    numOutputChannels = 2;   // 2 = master estéreo; 4 = master + cue (salidas 1-2 / 3-4)
    };

    class AudioEngine final
    {
    public:
        /** Máximo de canales de salida que maneja el motor (master L/R + cue L/R). */
        static constexpr int kMaxOutputChannels = 4;

        AudioEngine() = default;
        ~AudioEngine() = default;

        /** Reserva toda la memoria que va a usar process(). Hilo de control, audio detenido. */
        void prepare (const EngineConfig& config);

        /** Libera recursos. Hilo de control, audio detenido. */
        void release();

        /** Callback de audio. Llena 'numOutputs' canales de 'numSamples' muestras.
            Tolera punteros nulos y bloques más grandes que maxBlockSize. */
        void process (float* const* outputs, int numOutputs, int numSamples) noexcept;

        // --- Estado (seguro desde cualquier hilo) -------------------------------
        bool          isPrepared()          const noexcept { return prepared.load (std::memory_order_acquire); }
        double        getSampleRate()       const noexcept { return sampleRate.load (std::memory_order_relaxed); }
        int           getMaxBlockSize()     const noexcept { return maxBlockSize.load (std::memory_order_relaxed); }
        int           getNumOutputs()       const noexcept { return numOutputs.load (std::memory_order_relaxed); }
        std::uint64_t getProcessedBlocks()  const noexcept { return processedBlocks.load (std::memory_order_relaxed); }

    private:
        // Buffer de trabajo pre-reservado en prepare(); process() nunca reserva memoria.
        juce::AudioBuffer<float> scratch;

        std::atomic<bool>          prepared        { false };
        std::atomic<double>        sampleRate      { 0.0 };
        std::atomic<int>           maxBlockSize    { 0 };
        std::atomic<int>           numOutputs      { 0 };
        std::atomic<std::uint64_t> processedBlocks { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
} // namespace kb::engine
