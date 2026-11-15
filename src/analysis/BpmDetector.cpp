#include "analysis/BpmDetector.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace kb::analysis
{
    namespace
    {
        constexpr int    kFftOrder     = 9;                 // 512 puntos
        constexpr int    kFftSize      = 1 << kFftOrder;
        constexpr int    kHop          = 64;
        constexpr double kTargetRate   = 11025.0;           // submuestreo para el análisis
        constexpr int    kPhaseBins    = 64;

        bool cancelled (const std::atomic<bool>* c) noexcept
        {
            return c != nullptr && c->load (std::memory_order_relaxed);
        }

        /** Autocorrelación normalizada en un retardo fraccionario (interpolación lineal). */
        double acfAt (const std::vector<double>& acf, double lag) noexcept
        {
            const auto i = (size_t) lag;
            if (i + 1 >= acf.size())
                return 0.0;
            const auto f = lag - (double) i;
            return acf[i] * (1.0 - f) + acf[i + 1] * f;
        }

        /** "Pliega" la envolvente con un período (en cuadros) y mide cuán concentrados quedan los golpes. */
        double foldScore (const std::vector<float>& env, double period, int& bestBin) noexcept
        {
            double bins[kPhaseBins] = {};
            const auto inv = 1.0 / period;
            for (size_t t = 0; t < env.size(); ++t)
            {
                const auto ph = (double) t * inv;
                const auto b  = (int) ((ph - std::floor (ph)) * kPhaseBins) % kPhaseBins;
                bins[b] += env[t];
            }

            double best = -1.0, total = 0.0;
            bestBin = 0;
            for (int b = 0; b < kPhaseBins; ++b)
            {
                total += bins[b];
                const auto s = bins[(b + kPhaseBins - 1) % kPhaseBins] + bins[b] + bins[(b + 1) % kPhaseBins];
                if (s > best) { best = s; bestBin = b; }
            }
            return total > 0.0 ? best / total : 0.0;
        }
    }

    // =========================================================================
    //  1) Envolvente de onsets
    // =========================================================================
    std::vector<float> BpmDetector::onsetEnvelope (const float* left, const float* right, std::int64_t numFrames,
                                                   double sampleRate, double& envelopeRate, double& envelopeDelay,
                                                   const std::atomic<bool>* cancel, std::vector<float>* lowBand)
    {
        // Submuestreo simple (promedio) a ~11 kHz: alcanza para detectar golpes.
        const auto factor = std::max (1, (int) std::lround (sampleRate / kTargetRate));
        const auto rate   = sampleRate / factor;
        const auto n      = (std::int64_t) (numFrames / factor);

        std::vector<float> mono ((size_t) std::max<std::int64_t> (n, 0));
        for (std::int64_t i = 0; i < n; ++i)
        {
            float acc = 0.0f;
            const auto base = i * factor;
            for (int k = 0; k < factor; ++k)
                acc += left[base + k] + (right != nullptr ? right[base + k] : left[base + k]);
            mono[(size_t) i] = acc / (2.0f * (float) factor);
        }

        envelopeRate  = rate / kHop;
        envelopeDelay = (kFftSize * 0.5) / rate;

        if (n < kFftSize * 4)
            return {};

        juce::dsp::FFT fft (kFftOrder);
        juce::dsp::WindowingFunction<float> window (kFftSize, juce::dsp::WindowingFunction<float>::hann, false);

        const auto frames = (size_t) ((n - kFftSize) / kHop);
        std::vector<float> env (frames, 0.0f), low (frames, 0.0f);
        const auto lowBins = std::max (2, (int) std::lround (200.0 / (rate / kFftSize)));   // hasta ~200 Hz
        std::vector<float> buf ((size_t) kFftSize * 2);
        std::vector<float> prev ((size_t) kFftSize / 2 + 1, 0.0f);

        for (size_t f = 0; f < frames; ++f)
        {
            if ((f & 1023) == 0 && cancelled (cancel))
                return {};

            std::copy_n (mono.data() + f * kHop, kFftSize, buf.data());
            window.multiplyWithWindowingTable (buf.data(), kFftSize);
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);

            // Flujo espectral con compresión logarítmica, solo subidas de energía.
            float flux = 0.0f, lowFlux = 0.0f;
            for (int b = 1; b <= kFftSize / 2; ++b)
            {
                const auto mag = std::log1p (100.0f * buf[(size_t) b]);
                const auto d = mag - prev[(size_t) b];
                if (d > 0.0f)
                {
                    flux += d;
                    if (b <= lowBins)
                        lowFlux += d;
                }
                prev[(size_t) b] = mag;
            }
            env[f] = flux;
            low[f] = lowFlux;
        }

        // Quita la tendencia lenta (media móvil de ~0,4 s), deja solo los picos y normaliza.
        const auto win = std::max<size_t> (1, (size_t) (0.4 * envelopeRate));
        auto detrend = [win, frames] (const std::vector<float>& in)
        {
            std::vector<float> out (frames, 0.0f);
            double sum = 0.0;
            for (size_t i = 0; i < frames; ++i)
            {
                sum += in[i];
                if (i >= win)
                    sum -= in[i - win];
                const auto mean = (float) (sum / (double) std::min (i + 1, win));
                out[i] = std::max (0.0f, in[i] - mean);
            }
            const auto peak = *std::max_element (out.begin(), out.end());
            if (peak > 0.0f)
                for (auto& v : out)
                    v /= peak;
            return out;
        };

        if (lowBand != nullptr)
            *lowBand = detrend (low);
        return detrend (env);
    }

    // =========================================================================
    //  2) y 3) Tempo
    // =========================================================================
    BeatInfo BpmDetector::analyse (const float* left, const float* right, std::int64_t numFrames, double sampleRate,
                                   const std::atomic<bool>* cancel)
    {
        return analyse (left, right, numFrames, sampleRate, cancel, Options {});
    }

    BeatInfo BpmDetector::analyse (const float* left, const float* right, std::int64_t numFrames, double sampleRate,
                                   const std::atomic<bool>* cancel, const Options& opt)
    {
        BeatInfo result;
        if (left == nullptr || numFrames <= 0 || sampleRate <= 0.0)
            return result;

        double rate = 0.0, delay = 0.0;
        std::vector<float> lowEnv;
        const auto env = onsetEnvelope (left, right, numFrames, sampleRate, rate, delay, cancel, &lowEnv);
        if (env.size() < 64 || cancelled (cancel))
            return result;

        // --- Autocorrelación hasta 4 veces el período más largo ------------------------
        const auto maxLag = std::min (env.size() - 1, (size_t) std::ceil (4.0 * 60.0 * rate / opt.minBpm) + 2);
        const auto minLag = (size_t) std::floor (60.0 * rate / opt.maxBpm) / 2;
        std::vector<double> acf (maxLag + 1, 0.0);
        double energy = 0.0;
        for (auto v : env) energy += (double) v * v;
        if (energy <= 0.0)
            return result;

        for (size_t lag = std::max<size_t> (1, minLag); lag <= maxLag; ++lag)
        {
            double s = 0.0;
            for (size_t t = lag; t < env.size(); ++t)
                s += (double) env[t] * env[t - lag];
            acf[lag] = s / energy;
        }
        if (cancelled (cancel))
            return result;

        // --- Tempo aproximado: período + sus múltiplos ---------------------------------
        double bestScore = -1.0, coarseBpm = 0.0;
        for (double bpm = opt.minBpm; bpm < opt.maxBpm; bpm += 0.25)
        {
            const auto lag = 60.0 * rate / bpm;
            const auto score = acfAt (acf, lag) + 0.5 * acfAt (acf, 2.0 * lag) + 0.25 * acfAt (acf, 4.0 * lag);
            if (score > bestScore)
            {
                bestScore = score;
                coarseBpm = bpm;
            }
        }
        if (coarseBpm <= 0.0)
            return result;

        // --- Ajuste fino plegando toda la pista ----------------------------------------
        auto refine = [&] (double centre, double span, double step, double& bestBpm, int& bestBin)
        {
            double best = -1.0;
            for (double bpm = centre - span; bpm <= centre + span + 1.0e-9; bpm += step)
            {
                int bin = 0;
                const auto s = foldScore (env, 60.0 * rate / bpm, bin);
                if (s > best) { best = s; bestBpm = bpm; bestBin = bin; }
            }
            return best;
        };

        double bpm = coarseBpm;
        int bin = 0;
        refine (coarseBpm, 1.0, 0.01, bpm, bin);
        if (cancelled (cancel))
            return result;
        const auto peakiness = refine (bpm, 0.012, 0.001, bpm, bin);

        // Tempos "redondos": la mayoría de la música electrónica está en BPM enteros.
        if (std::abs (bpm - std::round (bpm)) < 0.03)
            bpm = std::round (bpm);
        bpm = std::clamp (bpm, opt.minBpm, opt.maxBpm);

        // Primer beat: fase del pliegue -> segundos (descontando el retardo de la envolvente).
        // Se usan los graves si marcan un pulso claro; si no (p. ej. tema sin bombo), todo el espectro.
        int finalBin = bin;
        const auto period = 60.0 * rate / bpm;
        int lowBin = 0;
        const auto lowScore = lowEnv.size() == env.size() ? foldScore (lowEnv, period, lowBin) : 0.0;
        if (lowScore > 2.0 * 3.0 / kPhaseBins)
            finalBin = lowBin;
        else
            foldScore (env, period, finalBin);
        const auto periodSeconds = 60.0 / bpm;
        const auto phaseFrames = ((finalBin + 0.5) / kPhaseBins) * (60.0 * rate / bpm);
        auto first = phaseFrames / rate + delay;
        first = std::fmod (first, periodSeconds);
        if (first < 0.0)
            first += periodSeconds;

        result.bpm = bpm;
        result.firstBeatSeconds = first;
        // Un pliegue al azar concentra ~3/64 en 3 bins; uno perfecto, casi todo.
        result.confidence = (float) std::clamp ((peakiness - 3.0 / kPhaseBins) / (1.0 - 3.0 / kPhaseBins), 0.0, 1.0);
        return result;
    }
} // namespace kb::analysis
