#pragma once
// =============================================================================
//  AudioEngine: raíz del motor de audio de KB - MUSIQUERO DJ.
//
//  No conoce el dispositivo de audio: recibe buffers de salida y los llena.
//  La capa de dispositivo (app/AudioDeviceService en Windows, Oboe en Android)
//  solo llama a prepare() / process() / release().
//
//  Flujo de señal (Fase 2):
//
//   Deck 1 ─▶ ChannelStrip ─┬─▶ fader · crossfader ──┐
//   (trim, EQ, filtro)      └─▶ CUE (pre-fader) ──┐  │
//   Deck 2 ─▶ ChannelStrip ─┬─▶ fader · crossfader ──┤
//                           └─▶ CUE (pre-fader) ──┤  ▼
//                                                 │  MASTER bus ─▶ volumen ─▶ limitador ─▶ salidas master
//                                                 ▼      │
//                                    CUE bus ─▶ CUE MIX ◀┘ ─▶ PHONES ─▶ limitador ─▶ salidas auriculares
//
//  Ruteo: master a las salidas 1-2 o 3-4 y auriculares a las otras. SPLIT:
//  todo por el par del master, izquierdo = master mono, derecho = cue mono.
//
//  Hilos:
//    * prepare() / release(): hilo de control, con el audio DETENIDO.
//    * process(): hilo de audio. Sin allocations, locks, I/O ni logs.
//    * API de control: UN solo hilo de control (el de mensajes). Comandos por
//      cola lock-free; parámetros continuos atómicos, suavizados en el audio.
//    * Las pistas que el audio deja de usar se liberan en collectGarbage().
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/ChannelStrip.h"
#include "engine/Deck.h"
#include "engine/LockFreeQueue.h"
#include "engine/MeterSource.h"
#include "engine/Sampler.h"
#include "engine/fx/FxUnit.h"
#include "engine/RealtimeChecks.h"
#include "engine/TrackBuffer.h"
#include "engine/dsp/Limiter.h"

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
        int    numOutputChannels = 2;   // 2 = estéreo; 4 = dos pares (master + auriculares)
    };

    enum class CrossfaderCurve : int
    {
        smooth  = 0,   // potencia constante: mezclas largas
        cut     = 1,   // corte: ambos al máximo salvo en el 10 % de cada punta
        scratch = 2    // scratch: corte casi instantáneo (2 %)
    };

    enum class EqBand : int { low = 0, mid = 1, high = 2 };

    enum class MeterId : int { channel1 = 0, channel2 = 1, masterLeft = 2, masterRight = 3, count = 4 };

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
        void process (float* const* outputs, int numOutputs, int numSamples) noexcept;

        // --- Transporte (hilo de mensajes) ------------------------------------------------
        bool loadTrack (int deck, std::shared_ptr<TrackBuffer> track);
        void ejectTrack (int deck);
        void play (int deck);
        void pause (int deck);
        void togglePlay (int deck);
        void cueDown (int deck);
        void cueUp (int deck);
        void seek (int deck, double seconds);

        // --- Performance (Fase 4) ------------------------------------------------------------
        void hotCue (int deck, int index);
        /** Graba (seconds >= 0) o borra (seconds < 0) un hot cue. */
        void setHotCue (int deck, int index, double seconds);
        void autoLoop (int deck, double beats);
        void loopRoll (int deck, double beats, bool on);
        void loopIn (int deck);
        void loopOut (int deck);
        void loopHalve (int deck);
        void loopDouble (int deck);
        void loopExit (int deck);
        void setSlip (int deck, bool on);
        void setReverse (int deck, bool on);
        void setDeckEffect (int deck, Deck::Effect effect, bool on);
        void scratchBegin (int deck);
        void scratchMove (int deck, double seconds);
        void scratchEnd (int deck);
        void nudge (int deck, double amount);

        // --- Sampler (ranuras 0..15: 0-7 deck 1, 8-15 deck 2) ------------------------------
        bool  loadSample (int slot, std::shared_ptr<TrackBuffer> sample);
        void  triggerSample (int slot);
        void  stopSample (int slot);
        void  setSampleVolume (int slot, float gain01) noexcept   { sampler.setVolume (slot, gain01); }
        float getSampleVolume (int slot) const noexcept           { return sampler.getVolume (slot); }
        void  setSampleLooping (int slot, bool loop) noexcept     { sampler.setLooping (slot, loop); }
        bool  getSampleLooping (int slot) const noexcept          { return sampler.isLooping (slot); }
        float getSampleProgress (int slot) const noexcept         { return sampler.getProgress (slot); }

        // --- Efectos (Fase 5): 2 unidades. Sus parámetros son atómicos (cualquier hilo).
        static constexpr int kNumFxUnits = 2;
        fx::FxUnit&       getFxUnit (int unit) noexcept       { return fxUnits[(size_t) juce::jlimit (0, kNumFxUnits - 1, unit)]; }
        const fx::FxUnit& getFxUnit (int unit) const noexcept { return fxUnits[(size_t) juce::jlimit (0, kNumFxUnits - 1, unit)]; }
        /** Tempo efectivo de lo que sale por un deck (BPM de la pista × velocidad), o 0. */
        double getDeckBpm (int deck) const noexcept;

        // --- Tempo y SYNC ------------------------------------------------------------------
        /** Tempo en % (-50..+50). Moverlo a mano desactiva el SYNC de ese deck. */
        void  setTempo (int deck, float percent) noexcept;
        /** Tempo actual en % (también lo cambia el SYNC). */
        float getTempo (int deck) const noexcept;
        /** SYNC: el deck sigue el tempo de la otra bandeja y se alinea con sus beats.
            Activarlo en un deck lo desactiva en el otro (la otra bandeja es el "master"). */
        void  toggleSync (int deck);
        void  setSync (int deck, bool enabled);
        bool  getSync (int deck) const noexcept;

        /** Relación de velocidad para igualar tempos, eligiendo mitad/doble si queda más
            cerca (p. ej. 140 vs 72 -> el deck lento va a 70, no a 140). */
        static double syncRatio (double masterEffectiveBpm, double slaveBpm) noexcept;
        static constexpr float kMaxTempoPercent = 50.0f;

        // --- AUTO MIX ---------------------------------------------------------------------
        /** Pasa de la bandeja que suena a la otra en 'seconds' segundos: iguala tempo y
            golpes (si los BPM están a menos de ±12 %), arranca la otra bandeja, mueve el
            crossfader con curva pareja, hace "bass swap" a la mitad y al final pausa la
            bandeja que salió. Si ya hay una mezcla en curso, no hace nada. */
        void  startAutoMix (double seconds);
        void  cancelAutoMix();
        bool  isAutoMixing() const noexcept { return autoMixProgress.load (std::memory_order_relaxed) >= 0.0f; }
        /** 0..1 durante la mezcla, -1 si no hay mezcla. */
        float getAutoMixProgress() const noexcept { return autoMixProgress.load (std::memory_order_relaxed); }
        /** Deck que está sonando "al aire" según el crossfader (0 o 1): el que sale en un AUTO MIX. */
        static int autoMixSource (float crossfader, bool reverse, bool playingA, bool playingB) noexcept;
        static constexpr double kAutoMixMaxTempoChange = 0.12;

        // --- Key Lock y key shift -----------------------------------------------------------
        /** Key Lock (master tempo): cambiar el tempo no cambia el tono. */
        void setKeyLock (int deck, bool enabled) noexcept;
        bool getKeyLock (int deck) const noexcept;
        /** Corrimiento de tono en semitonos (-12..+12), independiente del tempo. */
        void setKeyShift (int deck, int semitones) noexcept;
        int  getKeyShift (int deck) const noexcept;

        // --- Mixer (cualquier hilo; posiciones de perilla/fader 0..1) --------------------
        void  setChannelVolume (int deck, float position01) noexcept;
        float getChannelVolume (int deck) const noexcept;
        void  setTrim (int deck, float position01) noexcept;
        float getTrim (int deck) const noexcept;
        void  setEq (int deck, EqBand band, float position01) noexcept;
        float getEq (int deck, EqBand band) const noexcept;
        void  setFilter (int deck, float position01) noexcept;
        float getFilter (int deck) const noexcept;
        void  setChannelCue (int deck, bool enabled) noexcept;
        bool  getChannelCue (int deck) const noexcept;

        /** -1 (todo deck 1) .. +1 (todo deck 2). */
        void  setCrossfader (float position) noexcept;
        float getCrossfader() const noexcept { return crossfader.load (std::memory_order_relaxed); }
        void  setCrossfaderCurve (CrossfaderCurve c) noexcept { xfCurve.store ((int) c, std::memory_order_relaxed); }
        CrossfaderCurve getCrossfaderCurve() const noexcept { return (CrossfaderCurve) xfCurve.load (std::memory_order_relaxed); }
        void  setCrossfaderReverse (bool r) noexcept { xfReverse.store (r, std::memory_order_relaxed); }
        bool  getCrossfaderReverse() const noexcept  { return xfReverse.load (std::memory_order_relaxed); }

        void  setMasterVolume (float position01) noexcept;
        float getMasterVolume() const noexcept { return masterVolume.load (std::memory_order_relaxed); }

        /** Volumen de auriculares (0..1). */
        void  setHeadphoneVolume (float position01) noexcept;
        float getHeadphoneVolume() const noexcept { return phonesVolume.load (std::memory_order_relaxed); }
        /** CUE MIX: 0 = solo cue, 1 = solo master. */
        void  setCueMix (float position01) noexcept;
        float getCueMix() const noexcept { return cueMix.load (std::memory_order_relaxed); }

        // --- Ruteo de salidas ------------------------------------------------------------
        /** Par de salidas del master: 0 = salidas 1-2, 1 = salidas 3-4. Auriculares: el otro. */
        void setMasterOutputPair (int pair) noexcept { masterPair.store (juce::jlimit (0, 1, pair), std::memory_order_relaxed); }
        int  getMasterOutputPair() const noexcept    { return masterPair.load (std::memory_order_relaxed); }
        /** SPLIT: master (mono) a la izquierda y auriculares (mono) a la derecha del par del master. */
        void setSplit (bool enabled) noexcept { split.store (enabled, std::memory_order_relaxed); }
        bool getSplit() const noexcept        { return split.load (std::memory_order_relaxed); }

        // --- Medición (hilo de UI) --------------------------------------------------------
        MeterSource::Reading readMeter (MeterId id) noexcept;
        /** Reducción de ganancia del limitador del master, en dB (0 = no actúa). */
        float getMasterLimiterReductionDb() const noexcept { return limiterReductionDb.load (std::memory_order_relaxed); }

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
        /** Muestras que llegaron a la red final de ±1.0 (debería ser siempre 0). */
        std::uint64_t getClippedSamples()   const noexcept { return clippedSamples.load (std::memory_order_relaxed); }
        /** Latencia agregada por los limitadores, en muestras. */
        int           getLatencySamples()   const noexcept { return masterLimiter.getLatencySamples(); }

        // --- Curvas (públicas para pruebas y UI) ----------------------------------------
        static float faderGain (float position01) noexcept;
        static void  crossfaderGains (float position, float& gainA, float& gainB) noexcept;
        static void  crossfaderGains (float position, CrossfaderCurve curve, bool reverse,
                                      float& gainA, float& gainB) noexcept;

    private:
        struct Command
        {
            enum class Type : std::uint8_t
            {
                load, play, pause, togglePlay, cueDown, cueUp, seek, syncOn, syncOff, autoMixStart, autoMixStop,
                hotCue, setHotCue, autoLoop, loopRoll, loopIn, loopOut, loopHalve, loopDouble, loopExit,
                slip, reverse, effect, scratchBegin, scratchMove, scratchEnd, nudge,
                sampleLoad, sampleTrigger, sampleStop
            };
            Type         type  = Type::play;
            std::int8_t  deck  = 0;
            TrackBuffer* track = nullptr;
            double       value = 0.0;
            std::int8_t  index = 0;    // hot cue, efecto, ranura del sampler o on/off
        };

        // Canales del buffer de trabajo
        enum Bus { deck1L, deck1R, deck2L, deck2R, masterL, masterR, cueL, cueR, phonesL, phonesR, samplerL, samplerR, numBuses };

        bool send (const Command& c);
        void handleCommand (const Command& c) noexcept;
        void renderBlock (float* const* outputs, int numOutputs, int offset, int numSamples) noexcept;
        void updateTempo (int numSamples) noexcept;
        void beginAutoMix (double seconds) noexcept;
        void advanceAutoMix (int numSamples) noexcept;
        void endAutoMix (bool completed) noexcept;
        void engageSync (int deck) noexcept;
        /** Fase de beat del deck esclavo respecto del master (en beats del esclavo, -0.5..0.5). */
        bool syncPhaseError (int slave, double ratio, double& errorBeats) const noexcept;
        void returnRetiredTracks() noexcept;

        std::array<Deck, kNumDecks>         decks;
        std::array<ChannelStrip, kNumDecks> strips;
        Sampler sampler;
        std::array<fx::FxUnit, 2> fxUnits;
        bool sendDeck (Command::Type type, int deck, double value = 0.0, int index = 0);
        dsp::Limiter masterLimiter, phonesLimiter;
        std::array<MeterSource, 2> masterMeters;

        SpscQueue<Command, 256>      commands;
        SpscQueue<TrackBuffer*, 64>  released;
        std::vector<std::shared_ptr<TrackBuffer>> liveTracks;

        juce::AudioBuffer<float> work;   // [Bus][muestra], pre-reservado

        // Parámetros (control escribe, audio lee)
        struct ChannelParams
        {
            std::atomic<float> volume { 1.0f };
            std::atomic<float> trim   { 0.5f };
            std::atomic<float> low    { 0.5f };
            std::atomic<float> mid    { 0.5f };
            std::atomic<float> high   { 0.5f };
            std::atomic<float> filter { 0.5f };
            std::atomic<bool>  cue    { false };
            std::atomic<float> tempo  { 0.0f };    // %
            std::atomic<bool>  sync   { false };
            std::atomic<bool>  keyLock  { false };
            std::atomic<int>   keyShift { 0 };
        };
        std::array<ChannelParams, kNumDecks> channel;

        std::atomic<float> crossfader   { 0.0f };
        std::atomic<int>   xfCurve      { (int) CrossfaderCurve::smooth };
        std::atomic<bool>  xfReverse    { false };
        std::atomic<float> masterVolume { 0.85f };
        std::atomic<float> phonesVolume { 0.7f };
        std::atomic<float> cueMix       { 0.0f };
        std::atomic<int>   masterPair   { 0 };
        std::atomic<bool>  split        { false };

        // Suavizado (solo audio)
        std::array<juce::SmoothedValue<float>, kNumDecks> smoothedDeckGain, smoothedCueGain;
        juce::SmoothedValue<float> smoothedMaster, smoothedPhones, smoothedMixCue, smoothedMixMaster;

        std::atomic<bool>          prepared        { false };
        std::atomic<double>        sampleRate      { 0.0 };
        std::atomic<int>           maxBlockSize    { 0 };
        std::atomic<int>           numOutputs      { 0 };
        std::atomic<std::uint64_t> processedBlocks { 0 };
        std::atomic<std::uint64_t> clippedSamples  { 0 };
        std::atomic<float>         limiterReductionDb { 0.0f };

        // AUTO MIX (estado del hilo de audio; el progreso se publica)
        std::atomic<float> autoMixProgress { -1.0f };
        bool   amActive = false;
        int    amSource = 0, amTarget = 1;
        double amTotal = 0.0, amDone = 0.0;
        float  amStart = -1.0f, amEnd = 1.0f;
        std::array<float, kNumDecks> amLowMultiplier { 1.0f, 1.0f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
    };
} // namespace kb::engine
