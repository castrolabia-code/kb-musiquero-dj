// Pruebas de tonalidad: notación, Camelot, compatibilidad y detección sobre
// progresiones de acordes sintéticas con armónicos, bajo y algo de ruido.
#include <catch2/catch_test_macros.hpp>

#include "analysis/KeyDetector.h"

#include <cmath>
#include <random>
#include <vector>

using kb::analysis::KeyDetector;
using kb::analysis::MusicalKey;

namespace
{
    constexpr double kSr = 44100.0;

    double midiToHz (int midi) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }

    /** Progresión de acordes (cada acorde = notas MIDI), 2 s por acorde, con armónicos. */
    std::vector<float> progression (const std::vector<std::vector<int>>& chords, int repeats = 6)
    {
        const auto chordLen = (size_t) (2.0 * kSr);
        std::vector<float> x (chords.size() * (size_t) repeats * chordLen, 0.0f);
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

        size_t pos = 0;
        for (int r = 0; r < repeats; ++r)
        {
            for (const auto& chord : chords)
            {
                for (size_t i = 0; i < chordLen; ++i)
                {
                    const auto t = (double) i / kSr;
                    double s = 0.0;
                    for (auto note : chord)
                        for (int h = 1; h <= 4; ++h)   // timbre con armónicos
                            s += std::sin (2.0 * 3.141592653589793 * midiToHz (note) * h * t) / (h * h);
                    // Bajo: la fundamental del acorde una octava abajo.
                    s += 0.8 * std::sin (2.0 * 3.141592653589793 * midiToHz (chord.front() - 12) * t);
                    x[pos + i] = (float) (0.08 * s * std::exp (-1.2 * t)) + 0.01f * noise (rng);
                }
                pos += chordLen;
            }
        }
        return x;
    }

    int detect (const std::vector<std::vector<int>>& chords)
    {
        const auto x = progression (chords);
        return KeyDetector::analyse (x.data(), nullptr, (std::int64_t) x.size(), kSr).key;
    }
}

TEST_CASE ("Key: notacion musical y Camelot", "[analysis][key]")
{
    CHECK (MusicalKey::name (0) == "C");
    CHECK (MusicalKey::name (21) == "Am");
    CHECK (MusicalKey::name (13) == "C#m");
    CHECK (MusicalKey::camelot (0) == "8B");    // C
    CHECK (MusicalKey::camelot (7) == "9B");    // G
    CHECK (MusicalKey::camelot (5) == "7B");    // F
    CHECK (MusicalKey::camelot (21) == "8A");   // Am
    CHECK (MusicalKey::camelot (16) == "9A");   // Em
    CHECK (MusicalKey::camelot (14) == "7A");   // Dm
    CHECK (MusicalKey::camelot (6) == "2B");    // F#
    CHECK (MusicalKey::camelot (-1) == "--");
}

TEST_CASE ("Key: transposicion y compatibilidad armonica", "[analysis][key]")
{
    CHECK (MusicalKey::transpose (0, 2) == 2);      // C +2 = D
    CHECK (MusicalKey::transpose (21, 3) == 12);    // Am +3 = Cm
    CHECK (MusicalKey::transpose (0, -1) == 11);    // C -1 = B
    CHECK (MusicalKey::compatible (21, 0));         // 8A - 8B (relativas)
    CHECK (MusicalKey::compatible (0, 7));          // 8B - 9B
    CHECK (MusicalKey::compatible (0, 5));          // 8B - 7B
    CHECK (MusicalKey::compatible (11, 6));         // 1B - 2B
    CHECK_FALSE (MusicalKey::compatible (0, 2));    // 8B - 10B
    CHECK_FALSE (MusicalKey::compatible (0, 16));   // 8B - 9A
}

TEST_CASE ("Key: detecta Do mayor (C - F - G - C)", "[analysis][key]")
{
    CHECK (detect ({ { 60, 64, 67 }, { 65, 69, 72 }, { 67, 71, 74 }, { 60, 64, 67 } }) == 0);
}

TEST_CASE ("Key: detecta La menor (Am - Dm - E - Am)", "[analysis][key]")
{
    CHECK (detect ({ { 57, 60, 64 }, { 62, 65, 69 }, { 64, 68, 71 }, { 57, 60, 64 } }) == 21);
}

TEST_CASE ("Key: detecta Sol mayor y Mi menor", "[analysis][key]")
{
    CHECK (detect ({ { 55, 59, 62 }, { 60, 64, 67 }, { 62, 66, 69 }, { 55, 59, 62 } }) == 7);     // G - C - D - G
    CHECK (detect ({ { 52, 55, 59 }, { 57, 60, 64 }, { 59, 63, 66 }, { 52, 55, 59 } }) == 16);    // Em - Am - B - Em
}

TEST_CASE ("Key: silencio no inventa una tonalidad", "[analysis][key]")
{
    std::vector<float> silence ((size_t) (kSr * 10), 0.0f);
    CHECK_FALSE (KeyDetector::analyse (silence.data(), nullptr, (std::int64_t) silence.size(), kSr).isValid());
}
