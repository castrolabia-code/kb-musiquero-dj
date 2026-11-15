#pragma once
// =============================================================================
//  Biquad: filtro IIR de 2º orden (forma directa transpuesta II), con los
//  diseños del "Audio EQ Cookbook" (R. Bristow-Johnson).
//
//  Sin allocations: apto para el hilo de audio. Estado en double para que los
//  filtros de graves (250 Hz a 48 kHz) no acumulen error numérico.
// =============================================================================
#include <cmath>

namespace kb::engine::dsp
{
    struct BiquadCoefficients
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

        static constexpr double kButterworthQ = 0.70710678118654752440;

        static BiquadCoefficients lowPass (double sampleRate, double freq, double q = kButterworthQ) noexcept
        {
            const auto w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
            const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            return normalise ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
        }

        static BiquadCoefficients highPass (double sampleRate, double freq, double q = kButterworthQ) noexcept
        {
            const auto w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
            const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            return normalise ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
        }

        /** Pasa-todo de 2º orden: con Q = 1/sqrt(2) equivale a LP + HP de un cruce Linkwitz-Riley de 4º orden. */
        static BiquadCoefficients allPass (double sampleRate, double freq, double q = kButterworthQ) noexcept
        {
            const auto w = 2.0 * 3.14159265358979323846 * freq / sampleRate;
            const auto c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
            return normalise (1.0 - alpha, -2.0 * c, 1.0 + alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
        }

    private:
        static BiquadCoefficients normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
        {
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }
    };

    class Biquad
    {
    public:
        void setCoefficients (const BiquadCoefficients& c) noexcept { k = c; }
        void reset() noexcept { z1 = z2 = 0.0; }

        inline double process (double x) noexcept
        {
            const auto y = k.b0 * x + z1;
            z1 = k.b1 * x - k.a1 * y + z2;
            z2 = k.b2 * x - k.a2 * y;
            return y;
        }

    private:
        BiquadCoefficients k;
        double z1 = 0.0, z2 = 0.0;
    };
} // namespace kb::engine::dsp
