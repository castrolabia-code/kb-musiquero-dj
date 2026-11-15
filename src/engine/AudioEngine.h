#pragma once
// =============================================================================
//  AudioEngine: raíz del motor de audio de KB - MUSIQUERO DJ.
//
//  No conoce el dispositivo de audio: recibe buffers de salida y los llena.
//  La capa de dispositivo (app/AudioDeviceService en Windows, Oboe en Android)
//  solo llama a prepare() / process() / release().
//
//  Fase 1: 2 decks, volumen por canal, crossfader y master.
//
//  Hilos:
//    * prepare() / release(): hilo de control, con el audio DETENIDO.
//    * process(): hilo de audio. Sin allocations, locks, I/O ni logs.
//    * API de control (load, play, volumen...): UN solo hilo de control (el de
//      mensajes). Los comandos viajan por una cola lock-free; los parámetros
//      continuos son atómicos y se suavizan en el hilo de audio.
//    * Las pistas que el audio deja de usar vuelven por otra cola lock-free y
//      se liberan en collectGarbage() (hilo de control), nunca en el de audio.
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/Deck.h"
#include "engine/LockFreeQueue.h"
#include "engine/RealtimeChecks.h"
#include "engine/TrackBuffer.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

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
        static constexpr int kMaxOutputChannels = 4;
        static constexpr int kNumDecks          = 2;

        AudioEngine();
        ~AudioEngine();

        // --- Ciclo de vida (hilo de control, audio detenido) ---------------------------
        void prepare (const EngineConfig& config);
        void release();

        // --- Hilo de audio -------------------------------------------------------------
        /** Llena 'numOutputs' canales de 'numSamples' muestras.
            Tolera punteros nulos y bloques más grandes que maxBlockSize. */
        void process (float* const* outputs, int numOutputs, int numSamples) noexcept;

        // --- Control (hilo de mensajes) -------------------------------------------------
        /** Carga una pista en un deck. El motor guarda una referencia hasta que el
            audio la suelte. Devuelve false si la cola de comandos está llena. */
        bool loadTrack (int deck, std::shared_ptr<TrackBuffer> track);
        void ejectTrack (int deck);
        void play (int deck);
        void pause (int deck);
        void togglePlay (int deck);
        void cueDown (int deck);
        void cueUp (int deck);
        void seek (int deck, double seconds);

        /** 0..1 (posición del fader). */
        void  setChannelVolume (int deck, float position01) noexcept;
        float getChannelVolume (int deck) const noexcept;
        /** -1 (todo deck 1) .. +1 (todo deck 2). */
        void  setCrossfader (float position) noexcept;
        float getCrossfader() const noexcept { return crossfader.load (std::memory_order_relaxed); }
        /** 0..1 (posición de la perilla). */
        void  setMasterVolume (float position01) noexcept;
        float getMasterVolume() const noexcept { return masterVolume.load (std::memory_order_relaxed); }

        /** Libera las pistas que el audio ya soltó. Llamar periódicamente. */
        void collectGarbage();
        int  getNumLiveTracks() const noexcept { return (int) liveTracks.size(); }

        // --- Estado (cualquier hilo) -----------------------------------------------------
        Deck::Status  getDeckStatus (int deck) const noexcept;
        bool          isPrepared()          const noexcept { return prepared.load (std::memory_order_acquire); }
        double        getSampleRate()       const noexcept { return sampleRate.load (std::memory_order_relaxed); }
        int           getMaxBlockSize()     const noexcept { return maxBlockSize.load (std::memory_order_relaxed); }
        int           getNumOutputs()       const noexcept { return numOutputs.load (std::memory_order_relaxed); }
        std::uint64_t getProcessedBlocks()  const noexcept { return processedBlocks.load (std::memory_order_relaxed); }
        /** Muestras recortadas por el limitador de seguridad (diagnóstico). */
        std::uint64_t getClippedSamples()   const noexcept { return clippedSamples.load (std::memory_order_relaxed); }

        // --- Curvas (públicas para pruebas y UI) ----------------------------------------
        static float faderGain (float position01) noexcept;
        static void  crossfaderGains (float position, float& gainA, float& gainB) noexcept;

    private:
        struct Command
        {
            enum class Type : std::uint8_t { load, play, pause, togglePlay, cueDown, cueUp, seek };
            Type         type  = Type::play;
            std::int8_t  deck  = 0;
            TrackBuffer* track = nullptr;
            double       value = 0.0;
        };

        bool send (const Command& c);
        void handleCommand (const Command& c) noexcept;
        void renderBlock (float* const* outputs, int numOutputs, int offset, int numSamples) noexcept;
        void returnRetiredTracks() noexcept;

        std::array<Deck, kNumDecks> decks;

        SpscQueue<Command, 256>      commands;       // control -> audio
        SpscQueue<TrackBuffer*, 64>  released;       // audio -> control
        std::vector<std::shared_ptr<TrackBuffer>> liveTracks;   // solo hilo de control

        // Buffers de trabajo pre-reservados: [deck][canal][muestra]
        juce::AudioBuffer<float> deckBuffers;

        // Parámetros (control escribe, audio lee) y su suavizado (solo audio).
        std::array<std::atomic<float>, kNumDecks> channelVolume;
        std::atomic<float> crossfader   { 0.0f };
        std::atomic<float> masterVolume { 0.85f };

        std::array<juce::SmoothedValue<float>, kNumDecks> smoothedDeckGain;
        juce::SmoothedValue<float> smoothedMaster;

        std::atomic<bool>          prepared        { false };
        std::atomic<double>        sampleRate      { 0.0 };
        std::atomic<int>           maxBlockSize    { 0 };
        std::atomic<int>           numOutputs      { 0 };
        std::atomic<std::uint64_t> processedBlocks { 0 };
        std::atomic<std::uint64_t> clippedSamples  { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
} // namespace kb::engine
