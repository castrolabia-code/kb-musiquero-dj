#include "analysis/KeyDetector.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace kb::analysis
{
    namespace
    {
        // Perfiles de Krumhansl-Kessler (1982), empezando en la tónica.
        constexpr double kMajor[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        constexpr double kMinor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        const char* kNotes[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

        double correlation (const std::array<double, 12>& x, const double* profile, int rotation) noexcept
        {
            double mx = 0.0, my = 0.0;
            for (int i = 0; i < 12; ++i) { mx += x[(size_t) i]; my += profile[i]; }
            mx /= 12.0; my /= 12.0;

            double sxy = 0.0, sxx = 0.0, syy = 0.0;
            for (int i = 0; i < 12; ++i)
            {
                const auto dx = x[(size_t) ((i + rotation) % 12)] - mx;
                const auto dy = profile[i] - my;
                sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
            }
            return (sxx > 0.0 && syy > 0.0) ? sxy / std::sqrt (sxx * syy) : 0.0;
        }
    }

    // =========================================================================
    //  Notación
    // =========================================================================
    std::string MusicalKey::name (int key)
    {
        if (key < 0 || key > 23)
            return "--";
        return std::string (kNotes[tonic (key)]) + (isMinor (key) ? "m" : "");
    }

    int MusicalKey::camelotNumber (int key) noexcept
    {
        if (key < 0 || key > 23)
            return 0;
        // Mayores: C = 8B y cada quinta (+7 semitonos) suma 1. Menores: Am = 8A igual.
        const auto t = tonic (key);
        const auto ref = isMinor (key) ? 9 : 0;   // A menor / C mayor = 8
        const auto steps = ((t - ref + 12) * 7) % 12;   // cantidad de quintas desde la referencia
        return (8 - 1 + steps) % 12 + 1;
    }

    std::string MusicalKey::camelot (int key)
    {
        if (key < 0 || key > 23)
            return "--";
        return std::to_string (camelotNumber (key)) + (isMinor (key) ? "A" : "B");
    }

    int MusicalKey::transpose (int key, int semitones) noexcept
    {
        if (key < 0 || key > 23)
            return unknown;
        const auto t = ((tonic (key) + semitones) % 12 + 12) % 12;
        return (isMinor (key) ? 12 : 0) + t;
    }

    bool MusicalKey::compatible (int a, int b) noexcept
    {
        if (a < 0 || b < 0)
            return false;
        const auto na = camelotNumber (a), nb = camelotNumber (b);
        if (isMinor (a) == isMinor (b))
        {
            const auto d = std::abs (na - nb);
            return d == 0 || d == 1 || d == 11;
        }
        return na == nb;   // relativa mayor/menor (8A <-> 8B)
    }

    // =========================================================================
    //  Clasificación
    // =========================================================================
    KeyInfo KeyDetector::classify (const std::array<double, 12>& chroma)
    {
        KeyInfo r;
        double best = -2.0, second = -2.0;
        for (int k = 0; k < 24; ++k)
        {
            const auto c = correlation (chroma, k < 12 ? kMajor : kMinor, k % 12);
            if (c > best) { second = best; best = c; r.key = k; }
            else if (c > second) { second = c; }
        }
        r.confidence = (float) std::clamp ((best - second) * 5.0, 0.0, 1.0);
        if (best <= 0.0)
            r.key = MusicalKey::unknown;
        return r;
    }

    // =========================================================================
    //  Cromagrama
    // =========================================================================
    KeyInfo KeyDetector::analyse (const float* left, const float* right, std::int64_t numFrames, double sampleRate,
                                  const std::atomic<bool>* cancel)
    {
        constexpr int kOrder = 12, kSize = 1 << kOrder, kHop = 2048;
        if (left == nullptr || sampleRate <= 0.0)
            return {};

        const auto factor = std::max (1, (int) std::lround (sampleRate / 11025.0));
        const auto rate = sampleRate / factor;
        const auto n = numFrames / factor;
        if (n < kSize * 4)
            return {};

        std::vector<float> mono ((size_t) n);
        for (std::int64_t i = 0; i < n; ++i)
        {
            float acc = 0.0f;
            for (int k = 0; k < factor; ++k)
            {
                const auto j = i * factor + k;
                acc += left[j] + (right != nullptr ? right[j] : left[j]);
            }
            mono[(size_t) i] = acc / (2.0f * (float) factor);
        }

        // Clase de altura de cada bin (o -1 fuera de rango).
        std::vector<int> binClass ((size_t) kSize / 2 + 1, -1);
        for (int b = 1; b <= kSize / 2; ++b)
        {
            const auto f = b * rate / kSize;
            if (f < 55.0 || f > 2000.0)
                continue;
            const auto midi = 69.0 + 12.0 * std::log2 (f / 440.0);
            binClass[(size_t) b] = ((int) std::lround (midi) % 12 + 12) % 12;
        }

        juce::dsp::FFT fft (kOrder);
        juce::dsp::WindowingFunction<float> window (kSize, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> buf ((size_t) kSize * 2);
        std::array<double, 12> chroma {};

        for (std::int64_t pos = 0; pos + kSize <= n; pos += kHop)
        {
            if (cancel != nullptr && cancel->load (std::memory_order_relaxed))
                return {};

            std::copy_n (mono.data() + pos, kSize, buf.data());
            window.multiplyWithWindowingTable (buf.data(), kSize);
            fft.performFrequencyOnlyForwardTransform (buf.data(), true);

            std::array<double, 12> frame {};
            double total = 0.0;
            for (int b = 1; b <= kSize / 2; ++b)
            {
                const auto cls = binClass[(size_t) b];
                if (cls < 0)
                    continue;
                const auto m = (double) buf[(size_t) b];
                frame[(size_t) cls] += m;
                total += m;
            }
            // Cada cuadro aporta su "forma" (normalizada), comprimida para que los
            // pasajes fuertes no dominen todo el tema.
            if (total > 1.0e-6)
                for (int c = 0; c < 12; ++c)
                    chroma[(size_t) c] += std::sqrt (frame[(size_t) c] / total);
        }

        return classify (chroma);
    }
} // namespace kb::analysis
