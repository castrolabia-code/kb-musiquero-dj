#pragma once
// =============================================================================
//  Sampler: banco de samples que se disparan desde los pads (Fase 4).
//
//  16 ranuras (8 por deck). Cada una tiene su volumen y modo:
//    * one-shot: suena una vez; volver a tocar lo reinicia.
//    * loop:     se repite hasta que se vuelve a tocar (o se detiene).
//  Todo cambio brusco (inicio, reinicio, stop) usa un fundido de ~2 ms.
//  La salida se suma al bus MASTER (antes del volumen MASTER), como en los
//  controladores DJ: el sampler suena aunque el crossfader esté del otro lado.
//
//  Hilos: load/trigger/stop/render son del hilo de audio (los envía el motor por
//  su cola de comandos). Volumen y modo son atómicos (cualquier hilo).
// =============================================================================
#include "engine/TrackBuffer.h"

#include <array>
#include <atomic>

namespace kb::engine
{
    class Sampler final
    {
    public:
        static constexpr int kNumSlots = 16;

        Sampler();

        void prepare (double engineSampleRate) noexcept;

        // --- Hilo de audio ---------------------------------------------------------------
        void load (int slot, TrackBuffer* track) noexcept;
        void trigger (int slot) noexcept;
        void stop (int slot) noexcept;
        /** SUMA el sampler a L y R. */
        void render (float* left, float* right, int numSamples) noexcept;

        TrackBuffer* peekRetired() const noexcept { return numRetired > 0 ? retired[0] : nullptr; }
        void         dropRetired() noexcept;

        // --- Cualquier hilo ----------------------------------------------------------------
        void  setVolume (int slot, float gain01) noexcept;
        float getVolume (int slot) const noexcept;
        void  setLooping (int slot, bool loop) noexcept;
        bool  isLooping (int slot) const noexcept;
        bool  isPlaying (int slot) const noexcept;
        /** 0..1 de avance (para dibujar el pad), -1 si no suena. */
        float getProgress (int slot) const noexcept;

    private:
        struct Voice
        {
            TrackBuffer* track = nullptr;
            double pos = 0.0, ratio = 1.0;
            bool   playing = false, restart = false, stopping = false;
            float  fade = 0.0f;     // 0..1 (fundido de entrada/salida)
            float  gain = 0.0f;     // volumen suavizado
        };

        void retire (TrackBuffer* t) noexcept;

        double sampleRate = 48000.0;
        float  fadeStep = 1.0f / 96.0f;
        std::array<Voice, kNumSlots> voices;
        std::array<TrackBuffer*, kNumSlots * 2> retired {};
        int numRetired = 0;

        std::array<std::atomic<float>, kNumSlots> volume;
        std::array<std::atomic<bool>, kNumSlots>  looping;
        std::array<std::atomic<float>, kNumSlots> progress;
    };
} // namespace kb::engine
