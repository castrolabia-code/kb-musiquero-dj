// Pruebas del detector de BPM y grilla con ritmos sintéticos realistas
// (bombo, platillos, güiro, bajo, ruido de fondo y swing de humanización).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "analysis/BpmDetector.h"

#include <juce_core/juce_core.h>

#include <cmath>
#include <random>
#include <vector>

using kb::analysis::BeatInfo;
using kb::analysis::BpmDetector;
using Catch::Approx;

namespace
{
    constexpr double kSr = 44100.0;
    constexpr double kPi = 3.14159265358979323846;

    struct Pattern
    {
        double bpm         = 128.0;
        double seconds     = 90.0;
        double startOffset = 0.0;    // segundos antes del primer beat
        bool   fourOnFloor = true;   // bombo en todos los beats (house)
        bool   offbeatHats = true;   // platillo en los contratiempos
        bool   guiroEighths = false; // güiro en corcheas (cumbia)
        bool   bassOnOneThree = false;
        double jitterMs    = 0.0;    // humanización
        float  noiseLevel  = 0.03f;
    };

    std::vector<float> render (const Pattern& p)
    {
        const auto n = (size_t) (p.seconds * kSr);
        std::vector<float> x (n, 0.0f);
        std::mt19937 rng (42);
        std::normal_distribution<double> jitter (0.0, p.jitterMs / 1000.0);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

        auto addKick = [&] (double t, float amp)
        {
            const auto s0 = (size_t) (t * kSr);
            for (size_t i = 0; i < (size_t) (0.25 * kSr) && s0 + i < n; ++i)
            {
                const auto tt = (double) i / kSr;
                const auto f = 50.0 + 100.0 * std::exp (-tt * 30.0);   // barrido de bombo
                x[s0 + i] += amp * (float) (std::sin (2.0 * kPi * f * tt) * std::exp (-tt * 9.0));
            }
        };
        auto addNoiseHit = [&] (double t, float amp, double decay)
        {
            const auto s0 = (size_t) (t * kSr);
            for (size_t i = 0; i < (size_t) (0.12 * kSr) && s0 + i < n; ++i)
                x[s0 + i] += amp * noise (rng) * (float) std::exp (-(double) i / kSr * decay);
        };
        auto addBass = [&] (double t, float amp)
        {
            const auto s0 = (size_t) (t * kSr);
            for (size_t i = 0; i < (size_t) (0.3 * kSr) && s0 + i < n; ++i)
            {
                const auto tt = (double) i / kSr;
                x[s0 + i] += amp * (float) (std::sin (2.0 * kPi * 82.0 * tt) * std::exp (-tt * 6.0));
            }
        };

        const auto beat = 60.0 / p.bpm;
        int k = 0;
        for (double t = p.startOffset; t < p.seconds - 0.3; t += beat, ++k)
        {
            const auto j = [&] { return p.jitterMs > 0.0 ? jitter (rng) : 0.0; };
            if (p.fourOnFloor)                      addKick (t + j(), 0.8f);
            if (p.bassOnOneThree && k % 2 == 0)     addBass (t + j(), 0.6f);
            if (p.offbeatHats)                      addNoiseHit (t + beat * 0.5 + j(), 0.25f, 60.0);
            if (p.guiroEighths)
            {
                addNoiseHit (t + j(), 0.35f, 25.0);
                addNoiseHit (t + beat * 0.5 + j(), 0.3f, 25.0);
            }
        }
        for (auto& v : x)
            v += p.noiseLevel * noise (rng);
        return x;
    }

    BeatInfo detect (const Pattern& p)
    {
        const auto x = render (p);
        return BpmDetector::analyse (x.data(), nullptr, (std::int64_t) x.size(), kSr);
    }
}

TEST_CASE ("BPM: house a 128 exactos", "[analysis][bpm]")
{
    Pattern p;
    const auto r = detect (p);
    REQUIRE (r.isValid());
    CHECK (r.bpm == Approx (128.0).margin (0.05));
    CHECK (r.confidence > 0.3f);
}

TEST_CASE ("BPM: house con tempo no entero (124.5)", "[analysis][bpm]")
{
    Pattern p;
    p.bpm = 124.5;
    CHECK (detect (p).bpm == Approx (124.5).margin (0.05));
}

TEST_CASE ("BPM: cumbia a 95 con guiro en corcheas y bajo", "[analysis][bpm]")
{
    Pattern p;
    p.bpm = 95.0;
    p.fourOnFloor = false;
    p.offbeatHats = false;
    p.guiroEighths = true;
    p.bassOnOneThree = true;
    p.jitterMs = 6.0;   // tocada en vivo
    CHECK (detect (p).bpm == Approx (95.0).margin (0.1));
}

TEST_CASE ("BPM: cumbia rapida a 104.3", "[analysis][bpm]")
{
    Pattern p;
    p.bpm = 104.3;
    p.fourOnFloor = false;
    p.offbeatHats = false;
    p.guiroEighths = true;
    p.bassOnOneThree = true;
    CHECK (detect (p).bpm == Approx (104.3).margin (0.1));
}

TEST_CASE ("BPM: los tempos fuera de rango se pliegan a una octava (78-156)", "[analysis][bpm]")
{
    Pattern dnb;
    dnb.bpm = 174.0;
    dnb.seconds = 60.0;
    CHECK (detect (dnb).bpm == Approx (87.0).margin (0.1));

    Pattern slow;
    slow.bpm = 70.0;
    slow.seconds = 60.0;
    CHECK (detect (slow).bpm == Approx (140.0).margin (0.1));
}

TEST_CASE ("BPM: la grilla marca donde caen los beats", "[analysis][beatgrid]")
{
    Pattern p;
    p.startOffset = 0.3;
    const auto r = detect (p);
    REQUIRE (r.bpm == Approx (128.0).margin (0.05));
    const auto period = 60.0 / 128.0;
    // Distancia circular entre el primer beat detectado y el real.
    auto d = std::fmod (r.firstBeatSeconds - 0.3 + 10.0 * period, period);
    if (d > period * 0.5) d -= period;
    CHECK (std::abs (d) < 0.015);   // menos de 15 ms
}

TEST_CASE ("BPM: silencio o audio muy corto no inventa un tempo", "[analysis][bpm]")
{
    std::vector<float> silence ((size_t) (kSr * 10), 0.0f);
    CHECK_FALSE (BpmDetector::analyse (silence.data(), nullptr, (std::int64_t) silence.size(), kSr).isValid());

    std::vector<float> tiny (100, 0.5f);
    CHECK_FALSE (BpmDetector::analyse (tiny.data(), nullptr, 100, kSr).isValid());
}

TEST_CASE ("BPM: se puede cancelar", "[analysis][bpm]")
{
    Pattern p;
    const auto x = render (p);
    std::atomic<bool> cancel { true };
    CHECK_FALSE (BpmDetector::analyse (x.data(), nullptr, (std::int64_t) x.size(), kSr, &cancel).isValid());
}
