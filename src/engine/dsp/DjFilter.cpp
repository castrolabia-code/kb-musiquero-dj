#include "engine/dsp/DjFilter.h"

#include <algorithm>

namespace kb::engine::dsp
{
    static constexpr double kPi = 3.14159265358979323846;

    void DjFilter::prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        // Suavizado de la perilla: ~30 ms de constante de tiempo.
        const auto updatesPerSecond = sampleRate / kUpdateEvery;
        smoothing = (float) (1.0 - std::exp (-1.0 / (0.03 * updatesPerSecond)));
        current = target;
        reset();
        updateCoefficients();
    }

    void DjFilter::reset() noexcept
    {
        st[0] = st[1] = {};
        countdown = 0;
    }

    double DjFilter::cutoffFor (float p) noexcept
    {
        p = std::clamp (p, 0.0f, 1.0f);
        if (p < 0.5f)
        {
            // LPF: 20 kHz (casi centro) -> 60 Hz (todo a la izquierda), exponencial.
            const auto t = (0.5f - p) / 0.5f;
            return 20000.0 * std::pow (60.0 / 20000.0, (double) t);
        }
        // HPF: 20 Hz (casi centro) -> 8 kHz (todo a la derecha).
        const auto t = (p - 0.5f) / 0.5f;
        return 20.0 * std::pow (8000.0 / 20.0, (double) t);
    }

    void DjFilter::updateCoefficients() noexcept
    {
        const auto distance = std::abs (current - 0.5f);
        wet = std::clamp ((distance - 0.005f) / kCenterWidth, 0.0f, 1.0f);
        highPass = current > 0.5f;

        const auto fc = std::min (cutoffFor (current), sampleRate * 0.45);
        const auto g  = std::tan (kPi * fc / sampleRate);
        k  = 1.0 / kResonanceQ;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void DjFilter::process (float* left, float* right, int numSamples) noexcept
    {
        float* chans[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            if (--countdown <= 0)
            {
                countdown = kUpdateEvery;
                current += (target - current) * smoothing;
                if (std::abs (target - current) < 1.0e-5f)
                    current = target;
                updateCoefficients();
            }

            if (wet <= 0.0f)
            {
                // Bypass: igual se avanza el estado con la señal para que al entrar no haya salto.
                for (int c = 0; c < 2; ++c)
                {
                    const auto v0 = (double) chans[c][i];
                    const auto v3 = v0 - st[c].ic2;
                    const auto v1 = a1 * st[c].ic1 + a2 * v3;
                    const auto v2 = st[c].ic2 + a2 * st[c].ic1 + a3 * v3;
                    st[c].ic1 = 2.0 * v1 - st[c].ic1;
                    st[c].ic2 = 2.0 * v2 - st[c].ic2;
                }
                continue;
            }

            for (int c = 0; c < 2; ++c)
            {
                const auto v0 = (double) chans[c][i];
                const auto v3 = v0 - st[c].ic2;
                const auto v1 = a1 * st[c].ic1 + a2 * v3;
                const auto v2 = st[c].ic2 + a2 * st[c].ic1 + a3 * v3;
                st[c].ic1 = 2.0 * v1 - st[c].ic1;
                st[c].ic2 = 2.0 * v2 - st[c].ic2;

                const auto filtered = highPass ? (v0 - k * v1 - v2) : v2;
                chans[c][i] = (float) (v0 + (filtered - v0) * (double) wet);
            }
        }
    }
} // namespace kb::engine::dsp
