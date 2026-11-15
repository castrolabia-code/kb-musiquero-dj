#include "engine/dsp/ThreeBandIsolator.h"

#include <algorithm>

namespace kb::engine::dsp
{
    void ThreeBandIsolator::prepare (double sampleRate) noexcept
    {
        const auto lp1 = BiquadCoefficients::lowPass  (sampleRate, kLowMidHz);
        const auto hp1 = BiquadCoefficients::highPass (sampleRate, kLowMidHz);
        const auto lp2 = BiquadCoefficients::lowPass  (sampleRate, kMidHighHz);
        const auto hp2 = BiquadCoefficients::highPass (sampleRate, kMidHighHz);

        lp1a.setCoefficients (lp1); lp1b.setCoefficients (lp1);
        hp1a.setCoefficients (hp1); hp1b.setCoefficients (hp1);
        lp2a.setCoefficients (lp2); lp2b.setCoefficients (lp2);
        hp2a.setCoefficients (hp2); hp2b.setCoefficients (hp2);
        ap2.setCoefficients (BiquadCoefficients::allPass (sampleRate, kMidHighHz));
        reset();
    }

    void ThreeBandIsolator::reset() noexcept
    {
        for (auto* b : { &lp1a, &lp1b, &hp1a, &hp1b, &lp2a, &lp2b, &hp2a, &hp2b, &ap2 })
            b->reset();
    }

    float eqKnobToGain (float p) noexcept
    {
        p = std::clamp (p, 0.0f, 1.0f);
        if (p <= 0.01f)
            return 0.0f;   // KILL
        if (p < 0.5f)
        {
            // De -inf a 0 dB con curva cuadrática (25 % del recorrido = -12 dB).
            const auto t = p / 0.5f;
            return t * t;
        }
        // De 0 a +6 dB, lineal en dB.
        const auto db = 6.0f * (p - 0.5f) / 0.5f;
        return std::pow (10.0f, db / 20.0f);
    }
} // namespace kb::engine::dsp
