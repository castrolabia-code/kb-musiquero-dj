#include "engine/dsp/Limiter.h"

#include <algorithm>
#include <cmath>

namespace kb::engine::dsp
{
    void Limiter::prepare (double sampleRate, double lookaheadMs, double releaseMs, float c) noexcept
    {
        windowSize  = std::clamp ((int) std::lround (sampleRate * lookaheadMs / 1000.0), 2, kMaxLookahead);
        releaseCoef = 1.0 - std::exp (-1.0 / (sampleRate * releaseMs / 1000.0));
        ceiling     = c;
        reset();
    }

    void Limiter::reset() noexcept
    {
        delayL.fill (0.0f);
        delayR.fill (0.0f);
        boxValues.fill (1.0f);
        boxSum   = (double) windowSize;
        delayPos = boxPos = minHead = minCount = 0;
        envelope = 1.0;
        sampleIndex = 0;
    }

    float Limiter::process (float* left, float* right, int numSamples) noexcept
    {
        const int delay = windowSize - 1;
        float minApplied = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto l = left[i], r = right[i];

            // 1) Ganancia necesaria para esta muestra.
            const auto peak = std::max (std::abs (l), std::abs (r));
            const auto needed = peak > ceiling ? ceiling / peak : 1.0f;

            // 2) Mínimo en las últimas N muestras (deque monótono).
            while (minCount > 0)
            {
                const auto back = (minHead + minCount - 1) % kMaxLookahead;
                if (minValue[(size_t) back] >= needed) --minCount; else break;
            }
            {
                const auto slot = (minHead + minCount) % kMaxLookahead;
                minIndex[(size_t) slot] = sampleIndex;
                minValue[(size_t) slot] = needed;
                ++minCount;
            }
            while (minIndex[(size_t) minHead] <= sampleIndex - windowSize)
            {
                minHead = (minHead + 1) % kMaxLookahead;
                --minCount;
            }
            const auto windowMin = minValue[(size_t) minHead];

            // 3) Promedio deslizante del mínimo -> ataque suave dentro de la anticipación.
            boxSum += (double) windowMin - (double) boxValues[(size_t) boxPos];
            boxValues[(size_t) boxPos] = windowMin;
            boxPos = (boxPos + 1) % windowSize;
            const auto smoothed = std::min (1.0, boxSum / (double) windowSize);

            // 4) Liberación lenta (nunca por encima del valor seguro).
            if (smoothed < envelope)
                envelope = smoothed;
            else
                envelope += (smoothed - envelope) * releaseCoef;

            // 5) Aplicar a la señal retrasada.
            float dl = l, dr = r;
            if (delay > 0)
            {
                dl = delayL[(size_t) delayPos];
                dr = delayR[(size_t) delayPos];
                delayL[(size_t) delayPos] = l;
                delayR[(size_t) delayPos] = r;
                delayPos = (delayPos + 1) % delay;
            }

            const auto g = (float) envelope;
            // La red final de ±1.0 solo cubre errores de redondeo.
            left[i]  = std::clamp (dl * g, -1.0f, 1.0f);
            right[i] = std::clamp (dr * g, -1.0f, 1.0f);
            minApplied = std::min (minApplied, g);
            ++sampleIndex;
        }

        // Evita la deriva numérica del promedio en sesiones largas.
        if ((sampleIndex & 0xFFFF) < numSamples)
        {
            double exact = 0.0;
            for (int k = 0; k < windowSize; ++k)
                exact += (double) boxValues[(size_t) k];
            boxSum = exact;
        }

        return minApplied;
    }
} // namespace kb::engine::dsp
