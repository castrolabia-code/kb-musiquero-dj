#pragma once
// =============================================================================
//  MeterSource: mediciones para un vúmetro, del hilo de audio a la UI.
//
//  El audio acumula pico, suma de cuadrados y clip en cada bloque; la UI lee y
//  reinicia (read()) a su propio ritmo (60 Hz). Así no se pierde ningún pico
//  aunque entren varios bloques de audio entre dos cuadros de pantalla.
//  Solo atómicos lock-free: sin locks ni allocations.
// =============================================================================
#include <atomic>
#include <cmath>

namespace kb::engine
{
    class MeterSource
    {
    public:
        struct Reading
        {
            float peak    = 0.0f;   // lineal (1.0 = 0 dBFS)
            float rms     = 0.0f;   // lineal
            bool  clipped = false;
        };

        /** Hilo de audio. 'b' puede ser nullptr (medidor mono). */
        void addBlock (const float* a, const float* b, int n, float clipThreshold = 1.0f) noexcept
        {
            float pk = 0.0f;
            double sum = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const auto x = a[i];
                pk = std::fmax (pk, std::fabs (x));
                sum += (double) x * (double) x;
            }
            int count = n;
            if (b != nullptr)
            {
                for (int i = 0; i < n; ++i)
                {
                    const auto x = b[i];
                    pk = std::fmax (pk, std::fabs (x));
                    sum += (double) x * (double) x;
                }
                count += n;
            }

            auto prev = peak.load (std::memory_order_relaxed);
            while (pk > prev && ! peak.compare_exchange_weak (prev, pk, std::memory_order_relaxed)) {}
            sumSquares.fetch_add (sum, std::memory_order_relaxed);
            samples.fetch_add (count, std::memory_order_relaxed);
            if (pk >= clipThreshold)
                clip.store (true, std::memory_order_relaxed);
        }

        /** Hilo de audio: marca clip aunque la señal medida no lo muestre (p. ej. antes del limitador). */
        void markClip() noexcept { clip.store (true, std::memory_order_relaxed); }

        /** Hilo de UI: devuelve lo acumulado desde la última lectura y lo reinicia. */
        Reading read() noexcept
        {
            Reading r;
            r.peak    = peak.exchange (0.0f, std::memory_order_relaxed);
            const auto s = sumSquares.exchange (0.0, std::memory_order_relaxed);
            const auto n = samples.exchange (0, std::memory_order_relaxed);
            r.rms     = n > 0 ? (float) std::sqrt (s / (double) n) : 0.0f;
            r.clipped = clip.exchange (false, std::memory_order_relaxed);
            return r;
        }

    private:
        std::atomic<float>  peak       { 0.0f };
        std::atomic<double> sumSquares { 0.0 };
        std::atomic<int>    samples    { 0 };
        std::atomic<bool>   clip       { false };
    };
} // namespace kb::engine
