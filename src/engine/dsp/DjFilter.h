#pragma once
// =============================================================================
//  DjFilter: perilla FILTER combinada de mixer DJ.
//
//    perilla 0.0 ... 0.5 ... 1.0
//            LPF  | (sin efecto) |  HPF
//
//  Filtro de variable de estado "TPT" (Zavalishin / Simper): estable aunque la
//  frecuencia se mueva rápido, por eso se puede barrer sin clicks. Cerca del
//  centro se funde con la señal original, así no hay salto al entrar.
//  Resonancia moderada (Q = 1.0) para el sonido típico de filtro DJ.
// =============================================================================
#include <cmath>

namespace kb::engine::dsp
{
    class DjFilter
    {
    public:
        static constexpr double kResonanceQ   = 1.0;
        static constexpr float  kCenterWidth  = 0.03f;   // zona muerta a cada lado del centro
        static constexpr int    kUpdateEvery  = 16;      // muestras entre recálculos

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        /** Fija la posición de la perilla (0..1). Se suaviza internamente. */
        void setPosition (float position01) noexcept { target = position01; }

        /** Procesa un bloque estéreo en el lugar. */
        void process (float* left, float* right, int numSamples) noexcept;

        /** Frecuencia de corte (Hz) y modo para una posición (expuesto para pruebas). */
        static double cutoffFor (float position01) noexcept;

    private:
        struct State { double ic1 = 0.0, ic2 = 0.0; };
        void updateCoefficients() noexcept;

        double sampleRate = 48000.0;
        float  target = 0.5f, current = 0.5f;
        float  smoothing = 0.0f;   // coeficiente por actualización

        double a1 = 0.0, a2 = 0.0, a3 = 0.0, k = 1.0;
        bool   highPass = false;
        float  wet = 0.0f;
        State  st[2];
        int    countdown = 0;
    };
} // namespace kb::engine::dsp
