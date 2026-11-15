#pragma once
// =============================================================================
//  BpmDetector: detección de tempo (BPM) y de la grilla de beats.
//
//  Algoritmo propio en tres pasos:
//
//   1. Envolvente de "onsets" (golpes): flujo espectral sobre una versión
//      submuestreada (~11 kHz) de la pista, ~180 valores por segundo.
//   2. Tempo aproximado: autocorrelación de la envolvente, reforzada con los
//      múltiplos del período (2x, 4x) para evitar confundir 2/3 o 3/2 del tempo.
//      El rango de salida es una octava (78..156 BPM por defecto), así cada
//      tempo tiene una sola respuesta posible (174 -> 87, 70 -> 140).
//   3. Ajuste fino: para cada BPM candidato cerca del aproximado se "pliega"
//      toda la pista en un compás de un beat; el BPM correcto concentra los
//      golpes en la misma fase durante TODO el tema. Esto da precisión de
//      centésimas. La fase de la grilla se toma de los graves (bombo, bajo),
//      que marcan el beat; los platillos suelen ir a contratiempo.
//
//  Solo cálculo (sin UI ni dispositivos): corre en un hilo de fondo.
// =============================================================================
#include <atomic>
#include <cstdint>
#include <vector>

namespace kb::analysis
{
    struct BeatInfo
    {
        double bpm              = 0.0;   // 0 = no se pudo detectar
        double firstBeatSeconds = 0.0;   // posición de un beat en [0, 60/bpm)
        float  confidence       = 0.0f;  // 0..1 aprox.

        bool isValid() const noexcept { return bpm > 0.0; }
    };

    class BpmDetector
    {
    public:
        struct Options
        {
            double minBpm = 78.0;
            double maxBpm = 156.0;
        };

        /** Analiza audio estéreo (right puede ser nullptr). 'cancel' permite abortar. */
        static BeatInfo analyse (const float* left, const float* right, std::int64_t numFrames, double sampleRate,
                                 const std::atomic<bool>* cancel = nullptr);
        static BeatInfo analyse (const float* left, const float* right, std::int64_t numFrames, double sampleRate,
                                 const std::atomic<bool>* cancel, const Options& options);

        /** Envolvente de onsets (expuesta para pruebas). envelopeRate = valores por segundo;
            envelopeDelaySeconds = retardo de la envolvente respecto del audio. */
        static std::vector<float> onsetEnvelope (const float* left, const float* right, std::int64_t numFrames,
                                                 double sampleRate, double& envelopeRate, double& envelopeDelaySeconds,
                                                 const std::atomic<bool>* cancel = nullptr,
                                                 std::vector<float>* lowBandEnvelope = nullptr);
    };
} // namespace kb::analysis
