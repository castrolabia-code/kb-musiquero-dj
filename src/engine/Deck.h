#pragma once
// =============================================================================
//  Deck: reproductor de una pista. Vive en el hilo de audio.
//
//  Transporte con comportamiento CDJ estándar:
//    * PLAY/PAUSE alterna. Desde pausa reproduce desde la posición actual.
//    * CUE reproduciendo  -> vuelve al punto de cue y pausa.
//    * CUE en pausa, fuera del cue -> fija el cue en la posición actual.
//    * CUE en pausa, sobre el cue  -> preescucha mientras se mantiene apretado;
//      al soltar vuelve al cue y pausa. Si durante la preescucha se aprieta
//      PLAY, la reproducción sigue al soltar CUE.
//
//  Sin clicks: todo cambio brusco (play, pausa, salto, carga) se hace con un
//  fundido de ~3 ms. El audio que se interrumpe se sigue leyendo con un
//  segundo cabezal ("ghost") que se apaga mientras el principal entra.
//
//  Key Lock / key shift (Fase 3): con Key Lock encendido (o con el tono
//  corrido) el audio pasa por Signalsmith Stretch, que cambia la velocidad sin
//  cambiar el tono (o el tono sin cambiar la velocidad). La posición publicada
//  es siempre la que SE ESCUCHA (descontando la latencia del stretch), así el
//  SYNC alinea lo que suena de verdad.
//
//  Performance (Fase 4):
//    * 8 hot cues por pista.
//    * Loops: auto-loop por beats (cuantizado a la grilla), IN/OUT manual,
//      ½ / ×2, salir/reloop y loop roll. El salto al inicio del loop usa el
//      mismo fundido de 3 ms: sin clicks aunque el loop sea de 1/32 de beat.
//    * Slip: mientras dura un loop, scratch, reversa o backspin, una posición
//      "fantasma" sigue avanzando; al terminar, la pista salta ahí (como si
//      nunca se hubiera tocado).
//    * Plato: scratch (la posición sigue a la mano con un resorte suavizado)
//      y pitch bend (empujar/frenar el plato).
//    * Efectos de deck: freno (brake), backspin, censor (reversa con slip) y
//      reversa.
//    Mientras se manipula el audio (scratch, reversa, freno, backspin) el Key
//    Lock se suspende: esos efectos tienen que sonar como un vinilo.
//
//  Todos los métodos, salvo prepare() y los getters de estado, son del hilo de
//  audio: sin allocations, locks ni I/O.
// =============================================================================
#include "engine/TrackBuffer.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace kb::engine
{
    class Deck final
    {
    public:
        static constexpr double kFadeSeconds = 0.003;
        static constexpr int    kMaxRetired  = 8;
        static constexpr int    kNumHotCues  = 8;
        /** Tamaños de auto-loop válidos: 1/32 .. 32 beats. */
        static constexpr double kMinLoopBeats = 1.0 / 32.0, kMaxLoopBeats = 32.0;
        static constexpr double kBrakeSeconds = 1.2;

        enum class Effect : int { none = 0, brake = 1, backspin = 2, censor = 3 };

        Deck();
        ~Deck();

        /** Hilo de control, audio detenido. Reserva la memoria del time-stretch. */
        void prepare (double engineSampleRate, int maxBlockSize = 2048);

        // --- Comandos (hilo de audio) ------------------------------------------------
        /** Carga una pista (puede ser nullptr para vaciar el deck). La anterior queda
            "retirada" hasta que nadie la use; ver peekRetired(). */
        void load (TrackBuffer* newTrack) noexcept;
        void play() noexcept;
        void pause() noexcept;
        void togglePlay() noexcept;
        void cueDown() noexcept;
        void cueUp() noexcept;
        void seek (double seconds) noexcept;

        // --- Hot cues ------------------------------------------------------------------
        /** Si el cue está vacío lo graba en la posición actual; si no, salta ahí y reproduce. */
        void hotCue (int index) noexcept;
        /** Graba (seconds >= 0) o borra (seconds < 0) un hot cue sin moverse. */
        void setHotCue (int index, double seconds) noexcept;

        // --- Loops ---------------------------------------------------------------------
        /** Auto-loop de 'beats' beats desde el beat actual. Si ya hay un loop de ese
            tamaño, sale del loop (botón que alterna). */
        void autoLoop (double beats) noexcept;
        /** Loop roll: mientras se mantiene, loopea; al soltar sigue donde habría estado. */
        void loopRoll (double beats, bool on) noexcept;
        void loopInPoint() noexcept;
        /** OUT: cierra el loop manual; con un loop activo, sale; con uno guardado, reloop. */
        void loopOutPoint() noexcept;
        void loopHalve() noexcept;
        void loopDouble() noexcept;
        void loopExit() noexcept;

        // --- Slip, reversa, efectos de deck -----------------------------------------
        void setSlip (bool on) noexcept;
        void setReverse (bool on) noexcept;
        /** Efectos momentáneos (mientras se mantiene el pad). */
        void setEffect (Effect e, bool on) noexcept;

        // --- Plato ------------------------------------------------------------------------
        void scratchBegin() noexcept;
        /** Movimiento del plato (en segundos de pista, + adelante, - atrás). */
        void scratchMove (double seconds) noexcept;
        void scratchEnd() noexcept;
        /** Pitch bend momentáneo: fracción de velocidad (+0.05 = 5 % más rápido). Se
            desvanece solo en ~150 ms si no se sigue empujando. */
        void nudge (double amount) noexcept;
        /** Scratch, reversa o efecto en curso: el SYNC no corrige la fase. */
        bool isLooping() const noexcept { return loopActive; }
        bool isManipulated() const noexcept { return scratching || reverse || effect != Effect::none; }

        /** Key Lock (mantener el tono al cambiar el tempo) y key shift en semitonos. Hilo de audio. */
        void setPitchMode (bool keyLock, float keyShiftSemitones) noexcept;
        bool isStretching() const noexcept { return stretchActive; }

        /** Velocidad de reproducción (1 = normal, 1.08 = +8 %). Hilo de audio. */
        void   setRate (double r) noexcept { rate = r; }
        double getRate() const noexcept    { return rate; }

        // Acceso directo para el hilo de audio (SYNC).
        const TrackBuffer* getTrack() const noexcept    { return main.track; }
        bool   isPlaying() const noexcept               { return playing; }
        double getPositionSecondsRT() const noexcept    { return main.track != nullptr ? main.pos / main.track->getSampleRate() : 0.0; }

        /** Escribe n muestras (pisa el contenido) en L y R. */
        void render (float* left, float* right, int numSamples) noexcept;

        /** Lee un frame interpolado (Hermite) de una pista. También lo usa el sampler. */
        static void readFrame (const TrackBuffer& t, std::int64_t decoded, double pos, float& l, float& r) noexcept;

        /** Pistas que el deck ya no usa: el motor las devuelve al hilo de mensajes. */
        TrackBuffer* peekRetired() const noexcept { return numRetired > 0 ? retired[0] : nullptr; }
        void         dropRetired() noexcept;

        // --- Estado publicado (cualquier hilo) -------------------------------------------
        struct Status
        {
            std::uint64_t trackId         = 0;
            bool          playing         = false;
            bool          cuePreview      = false;
            double        positionSeconds = 0.0;
            double        cueSeconds      = 0.0;
            std::array<double, kNumHotCues> hotCues {};   // segundos; < 0 = vacío
            bool          loopActive      = false;
            double        loopInSeconds   = -1.0;
            double        loopOutSeconds  = -1.0;
            bool          slipMode        = false;
            bool          slipping        = false;
            double        slipSeconds     = 0.0;
            bool          reverse         = false;
            bool          scratching      = false;
            bool          rolling         = false;
            int           effect          = 0;
        };
        Status getStatus() const noexcept;

    private:
        struct Head
        {
            TrackBuffer* track = nullptr;
            double       pos   = 0.0;   // en frames de la pista
            double       ratio = 1.0;   // frames de pista por muestra de salida
        };

        struct Stretcher;

        void startGhost() noexcept;
        void prime() noexcept;
        double stretchLead() const noexcept;   // frames de pista entre lo que se lee y lo que se escucha
        bool renderStretched (int numSamples, std::int64_t decoded) noexcept;
        void endGhost() noexcept;
        void retire (TrackBuffer* t) noexcept;
        void publish() noexcept;
        double ratioFor (const TrackBuffer* t) const noexcept;

        void jumpTo (double frames) noexcept;
        void updateSlip() noexcept;
        bool loopValid() const noexcept { return loopIn >= 0.0 && loopOut > loopIn; }
        double framesPerBeat() const noexcept;
        void setLoop (double inFrames, double beats) noexcept;
        double nextAdvance() noexcept;
        void wrapLoop (double previousPos, double advance) noexcept;

        double engineSampleRate = 48000.0;
        int    fadeLength       = 144;
        float  fadeStep         = 1.0f / 144.0f;

        // Cabezal principal
        Head  main;
        float mainGain   = 0.0f;
        bool  playing    = false;
        bool  cuePreview = false;
        double cuePoint  = 0.0;     // frames

        // Cabezal que se apaga (fundido de salida)
        Head  ghost;
        float ghostGain      = 0.0f;
        float ghostStep      = 0.0f;
        double ghostRate     = 1.0;
        double rate          = 1.0;
        int   ghostRemaining = 0;
        std::int64_t  ghostDecoded = 0;
        std::uint32_t ghostSerial = 0;   // cambia en cada startGhost(): evita sumar el fantasma nuevo en la misma muestra del salto
        float motionGain = 1.0f;         // baja el volumen cuando el disco casi no se mueve (como un vinilo)

        // Time-stretch (Key Lock / key shift)
        std::unique_ptr<Stretcher> stretcher;
        bool   keyLock = false, stretchActive = false, needPrime = true;
        float  keyShift = 0.0f, appliedTranspose = 1000.0f;
        double stretchReadPos = 0.0;   // frames de pista: hasta dónde se le dio entrada al stretch
        double inputFraction  = 0.0;
        bool   usingStretch   = false;
        double effRate        = 1.0;   // rate con el pitch bend aplicado

        // Performance
        std::array<double, kNumHotCues> hotCues {};
        double loopIn = -1.0, loopOut = -1.0;
        bool   loopActive = false, rolling = false, manualInPending = false;
        double loopBeats = 0.0;
        bool   slipMode = false, slipTracking = false;
        double slipPos = 0.0;
        bool   reverse = false, scratching = false;
        Effect effect = Effect::none;
        double scratchTarget = 0.0, scratchVel = 0.0, scratchK = 0.0, scratchSmooth = 0.0;
        double brakeSpeed = 1.0, brakeStep = 0.0, spinSpeed = 0.0, spinDecay = 0.0;
        double bend = 0.0, bendTarget = 0.0;

        std::array<TrackBuffer*, kMaxRetired> retired {};
        int numRetired = 0;

        // Estado para la UI
        std::atomic<std::uint64_t> pubTrackId    { 0 };
        std::atomic<bool>          pubPlaying    { false };
        std::atomic<bool>          pubCuePreview { false };
        std::atomic<double>        pubPosition   { 0.0 };
        std::atomic<double>        pubCue        { 0.0 };
        std::array<std::atomic<double>, kNumHotCues> pubHotCues {};
        std::atomic<bool>          pubLoopActive { false }, pubSlipMode { false }, pubSlipping { false };
        std::atomic<bool>          pubReverse { false }, pubScratching { false }, pubRolling { false };
        std::atomic<double>        pubLoopIn { -1.0 }, pubLoopOut { -1.0 }, pubSlip { 0.0 };
        std::atomic<int>           pubEffect { 0 };
    };
} // namespace kb::engine
