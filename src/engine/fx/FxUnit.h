#pragma once
// =============================================================================
//  FxUnit: una unidad de efectos (Fase 5). El motor tiene dos.
//
//  Cada unidad: un efecto elegido, ON/OFF, DRY/WET, un parámetro principal
//  (distinto para cada efecto) y la duración en beats (1/8 .. 8) para los
//  efectos sincronizados al BPM. Se asigna al deck 1, al deck 2 o al master.
//
//    Efecto       Parámetro            Beats
//    ECHO         realimentación       tiempo del eco (con filtro: suena "a cinta")
//    DELAY        realimentación       tiempo (ping-pong estéreo)
//    REVERB       tamaño de la sala    -
//    FLANGER      realimentación       período del barrido
//    PHASER       realimentación       período del barrido
//    FILTER       profundidad          período del barrido (filtro resonante)
//    BITCRUSHER   destrucción          -
//    GATER        profundidad          largo de cada golpe
//    ROLL         -                    largo del fragmento que se repite
//    BRAKE        -                    duración de la frenada
//    BACKSPIN     -                    duración del giro hacia atrás
//    NOISE        frecuencia del ruido -
//
//  Echo, Delay y Reverb dejan sonar la cola al apagarlos (spill-over).
//  Cambiar de efecto con la unidad encendida hace un fundido corto (sin clicks).
//
//  Hilos: los parámetros son atómicos (los escribe la UI); process() es del
//  hilo de audio: sin allocations (todo se reserva en prepare()).
// =============================================================================
#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace kb::engine::fx
{
    enum class Type : int { echo, delay, reverb, flanger, phaser, filter, bitcrusher, gater, roll, brake, backspin, noise, count };
    enum class Target : int { deck1 = 0, deck2 = 1, master = 2 };

    /** Divisiones de beat disponibles (índice 0..kNumBeats-1). */
    inline constexpr std::array<double, 7> kBeats { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
    inline constexpr int kDefaultBeatsIndex = 3;   // 1 beat

    class FxUnit final
    {
    public:
        static constexpr double kMaxBufferSeconds = 9.0;   // 8 beats a 60 BPM, con margen

        FxUnit();

        /** Hilo de control, audio detenido. */
        void prepare (double sampleRate, int maxBlockSize);

        /** Procesa en el lugar. bpm = tempo del material que pasa por la unidad. */
        void process (float* left, float* right, int numSamples, double bpm) noexcept;

        /** true si la unidad está procesando (encendida o sonando una cola). */
        bool isActive() const noexcept { return active; }

        // --- Parámetros (cualquier hilo) --------------------------------------------------
        void   setType (Type t) noexcept      { type.store ((int) t, std::memory_order_relaxed); }
        Type   getType() const noexcept       { return (Type) type.load (std::memory_order_relaxed); }
        void   setOn (bool on) noexcept       { enabled.store (on, std::memory_order_relaxed); }
        bool   isOn() const noexcept          { return enabled.load (std::memory_order_relaxed); }
        void   setWet (float w) noexcept      { wet.store (juce::jlimit (0.0f, 1.0f, w), std::memory_order_relaxed); }
        float  getWet() const noexcept        { return wet.load (std::memory_order_relaxed); }
        void   setParam (float p) noexcept    { param.store (juce::jlimit (0.0f, 1.0f, p), std::memory_order_relaxed); }
        float  getParam() const noexcept      { return param.load (std::memory_order_relaxed); }
        void   setBeatsIndex (int i) noexcept { beatsIndex.store (juce::jlimit (0, (int) kBeats.size() - 1, i), std::memory_order_relaxed); }
        int    getBeatsIndex() const noexcept { return beatsIndex.load (std::memory_order_relaxed); }
        void   setTarget (Target t) noexcept  { target.store ((int) t, std::memory_order_relaxed); }
        Target getTarget() const noexcept     { return (Target) target.load (std::memory_order_relaxed); }

        static bool hasTail (Type t) noexcept { return t == Type::echo || t == Type::delay || t == Type::reverb; }
        static bool usesBeats (Type t) noexcept;

    private:
        void resetState() noexcept;
        void startTrigger() noexcept;
        float readBuffer (const std::vector<float>& buf, double pos) const noexcept;

        // Parámetros
        std::atomic<int>   type { (int) Type::echo };
        std::atomic<bool>  enabled { false };
        std::atomic<float> wet { 0.5f }, param { 0.5f };
        std::atomic<int>   beatsIndex { kDefaultBeatsIndex };
        std::atomic<int>   target { (int) Target::master };

        // Estado del audio
        double sr = 48000.0;
        Type   applied = Type::echo;
        bool   active = false, wasOn = false;
        float  onGain = 0.0f, onStep = 0.0f;       // fundido de encendido / cambio de efecto
        juce::SmoothedValue<float> wetSmooth, paramSmooth;
        double tailSilence = 0.0;                   // segundos de cola en silencio

        // Buffer circular estéreo compartido (eco, delay, flanger, roll, brake, backspin)
        std::vector<float> bufL, bufR;
        int    size = 1, writePos = 0;

        // Echo / delay
        float  fbLpL = 0.0f, fbLpR = 0.0f;
        double delaySmoothed = 0.0;

        // LFO (flanger, phaser, filter)
        double lfoPhase = 0.0;

        // Phaser: 6 pasa-todos por canal
        std::array<float, 6> apL {}, apR {};
        float phaserFbL = 0.0f, phaserFbR = 0.0f;

        // Filtro resonante (TPT SVF)
        double svf1L = 0.0, svf2L = 0.0, svf1R = 0.0, svf2R = 0.0;

        // Bitcrusher
        float holdL = 0.0f, holdR = 0.0f;
        double holdCounter = 0.0;

        // Gater
        double gatePhase = 0.0;
        float  gateGain = 1.0f;

        // Roll / brake / backspin
        int    triggerStart = 0;
        double triggerPos = 0.0, triggerSpeed = 1.0, triggerElapsed = 0.0;

        // Noise
        juce::Random noiseRng { 0x4B42 };
        double nz1L = 0.0, nz2L = 0.0, nz1R = 0.0, nz2R = 0.0;

        juce::Reverb reverb;
        std::vector<float> scratchL, scratchR;
    };
} // namespace kb::engine::fx
