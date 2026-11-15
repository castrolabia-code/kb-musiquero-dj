#pragma once
// =============================================================================
//  Limiter: limitador de picos "brickwall" con anticipación (lookahead).
//
//  Garantiza que la salida NUNCA supere el techo (ceiling):
//    1. Para cada muestra se calcula la ganancia necesaria  g = techo / pico.
//    2. Se toma el mínimo de g en una ventana de N muestras (anticipación).
//    3. Ese mínimo se promedia en la misma ventana (ataque suave, sin
//       distorsión) y se aplica a la señal retrasada N-1 muestras.
//       Cada valor promediado es <= a la ganancia que necesita esa muestra.
//    4. La liberación (subida de la ganancia) es exponencial y lenta.
//  Por debajo del techo la señal pasa intacta (solo retrasada ~2 ms).
//
//  Sin allocations fuera de prepare(): apto para el hilo de audio.
// =============================================================================
#include <array>
#include <cstdint>

namespace kb::engine::dsp
{
    class Limiter
    {
    public:
        static constexpr int   kMaxLookahead   = 1024;
        static constexpr float kDefaultCeiling = 0.966f;   // -0.3 dBFS

        void prepare (double sampleRate, double lookaheadMs = 2.0, double releaseMs = 150.0,
                      float ceiling = kDefaultCeiling) noexcept;
        void reset() noexcept;

        /** Procesa en el lugar. Devuelve la menor ganancia aplicada en el bloque (1 = nada). */
        float process (float* left, float* right, int numSamples) noexcept;

        int   getLatencySamples() const noexcept { return windowSize - 1; }
        float getCeiling() const noexcept        { return ceiling; }

    private:
        int    windowSize = 96;      // N
        float  ceiling    = kDefaultCeiling;
        double releaseCoef = 0.001;
        double envelope    = 1.0;

        // Retardo de la señal (N-1 muestras)
        std::array<float, kMaxLookahead> delayL {}, delayR {};
        int delayPos = 0;

        // Mínimo deslizante (deque monótono en un anillo)
        std::array<std::int64_t, kMaxLookahead> minIndex {};
        std::array<float, kMaxLookahead>        minValue {};
        int minHead = 0, minCount = 0;

        // Promedio deslizante del mínimo
        std::array<float, kMaxLookahead> boxValues {};
        double boxSum = 0.0;
        int boxPos = 0;

        std::int64_t sampleIndex = 0;
    };
} // namespace kb::engine::dsp
