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

        static void readFrame (const TrackBuffer& t, std::int64_t decoded, double pos, float& l, float& r) noexcept;

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

        // Time-stretch (Key Lock / key shift)
        std::unique_ptr<Stretcher> stretcher;
        bool   keyLock = false, stretchActive = false, needPrime = true;
        float  keyShift = 0.0f, appliedTranspose = 1000.0f;
        double stretchReadPos = 0.0;   // frames de pista: hasta dónde se le dio entrada al stretch
        double inputFraction  = 0.0;

        std::array<TrackBuffer*, kMaxRetired> retired {};
        int numRetired = 0;

        // Estado para la UI
        std::atomic<std::uint64_t> pubTrackId    { 0 };
        std::atomic<bool>          pubPlaying    { false };
        std::atomic<bool>          pubCuePreview { false };
        std::atomic<double>        pubPosition   { 0.0 };
        std::atomic<double>        pubCue        { 0.0 };
    };
} // namespace kb::engine
