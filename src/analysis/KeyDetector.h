#pragma once
// =============================================================================
//  KeyDetector: tonalidad de una pista (algoritmo propio).
//
//   1. Cromagrama: FFT de 4096 puntos a ~11 kHz (resolución ~2,7 Hz) entre
//      55 Hz y 2 kHz; cada bin suma su energía (comprimida) a una de las 12
//      clases de altura (do, do#, re...) según su frecuencia (A4 = 440 Hz).
//   2. Perfiles de Krumhansl-Schmuckler (Krumhansl & Kessler, 1982): se
//      correlaciona el cromagrama promedio con los 24 perfiles (12 mayores y
//      12 menores, rotados). La correlación más alta es la tonalidad.
//
//  Notación musical ("Am", "F#") y Camelot ("8A", "2B") para mezcla armónica.
// =============================================================================
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace kb::analysis
{
    /** Tonalidad codificada 0..23: 0..11 = Do..Si mayor, 12..23 = Do..Si menor. -1 = desconocida. */
    struct MusicalKey
    {
        static constexpr int unknown = -1;

        static bool isMinor (int key) noexcept      { return key >= 12; }
        static int  tonic (int key) noexcept        { return key % 12; }

        /** "C", "F#", "Am", "C#m"... */
        static std::string name (int key);
        /** "8B", "8A"... (rueda de Camelot) */
        static std::string camelot (int key);
        /** Transpone n semitonos (key shift). */
        static int transpose (int key, int semitones) noexcept;
        /** Compatibles para mezclar: misma, ±1 en la rueda, o relativa mayor/menor. */
        static bool compatible (int a, int b) noexcept;
        /** Número de Camelot (1..12). */
        static int camelotNumber (int key) noexcept;
    };

    struct KeyInfo
    {
        int   key = MusicalKey::unknown;
        float confidence = 0.0f;   // diferencia con la segunda mejor (0..1 aprox.)
        bool  isValid() const noexcept { return key >= 0; }
    };

    class KeyDetector
    {
    public:
        static KeyInfo analyse (const float* left, const float* right, std::int64_t numFrames, double sampleRate,
                                const std::atomic<bool>* cancel = nullptr);

        /** Correlación de un cromagrama (12 valores) con los 24 perfiles (expuesto para pruebas). */
        static KeyInfo classify (const std::array<double, 12>& chroma);
    };
} // namespace kb::analysis
