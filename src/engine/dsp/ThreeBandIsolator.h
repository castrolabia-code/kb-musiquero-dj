#pragma once
// =============================================================================
//  ThreeBandIsolator: EQ de 3 bandas tipo "isolator" de mixer de club.
//
//  La señal se divide con cruces Linkwitz-Riley de 4º orden (24 dB/oct) en
//  graves (< 250 Hz), medios y agudos (> 3 kHz), y cada banda tiene su propia
//  ganancia. Con las tres en 1 la suma es plana (solo cambia la fase, como en
//  cualquier mixer analógico). Ganancia 0 = KILL: la banda desaparece.
//
//        x ──┬── LP4(f1) ── AP(f2) ─────────────── · gLow  ──┐
//            └── HP4(f1) ──┬── LP4(f2) ──────────── · gMid  ──┼── y
//                          └── HP4(f2) ──────────── · gHigh ──┘
//
//  El pasa-todo AP(f2) en los graves compensa la fase del segundo cruce para
//  que la suma siga siendo plana.
// =============================================================================
#include "engine/dsp/Biquad.h"

namespace kb::engine::dsp
{
    class ThreeBandIsolator
    {
    public:
        static constexpr double kLowMidHz  = 250.0;
        static constexpr double kMidHighHz = 3000.0;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        /** Procesa un canal en el lugar con ganancias lineales por banda (constantes en el bloque). */
        inline double process (double x, double gLow, double gMid, double gHigh) noexcept
        {
            const auto low  = ap2.process (lp1b.process (lp1a.process (x)));
            const auto rest = hp1b.process (hp1a.process (x));
            const auto mid  = lp2b.process (lp2a.process (rest));
            const auto high = hp2b.process (hp2a.process (rest));
            return low * gLow + mid * gMid + high * gHigh;
        }

    private:
        Biquad lp1a, lp1b, hp1a, hp1b, lp2a, lp2b, hp2a, hp2b, ap2;
    };

    /** Posición de perilla (0..1) -> ganancia lineal. 0 = kill, 0.5 = 0 dB, 1 = +6 dB. */
    float eqKnobToGain (float position01) noexcept;
} // namespace kb::engine::dsp
