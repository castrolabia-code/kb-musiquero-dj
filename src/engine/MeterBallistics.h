#pragma once
// =============================================================================
//  MeterBallistics: comportamiento visual de un vúmetro digital realista.
//
//    * Pico: ataque instantáneo, caída de 18 dB/s.
//    * RMS:  ataque de ~50 ms, caída de 12 dB/s (el "cuerpo" de la barra).
//    * Retención de pico: se queda 1,5 s y luego cae a 8 dB/s.
//    * Clip: queda encendido 2 s.
//  Trabaja en dBFS con piso en -60 dB. Pura lógica (sin UI): se prueba sola.
// =============================================================================
#include <algorithm>
#include <cmath>

namespace kb::engine
{
    class MeterBallistics
    {
    public:
        static constexpr float kFloorDb           = -60.0f;
        static constexpr float kPeakReleaseDbPerS = 18.0f;
        static constexpr float kRmsAttackSeconds  = 0.05f;
        static constexpr float kRmsReleaseDbPerS  = 12.0f;
        static constexpr float kHoldSeconds       = 1.5f;
        static constexpr float kHoldFallDbPerS    = 8.0f;
        static constexpr float kClipHoldSeconds   = 2.0f;

        static float toDb (float linear) noexcept
        {
            return linear > 0.0f ? std::max (kFloorDb, 20.0f * std::log10 (linear)) : kFloorDb;
        }

        void update (float peakLinear, float rmsLinear, bool clipped, float dt) noexcept
        {
            dt = std::max (0.0f, dt);
            const auto pk = toDb (peakLinear);
            const auto rm = toDb (rmsLinear);

            peakDb = pk >= peakDb ? pk : std::max (pk, peakDb - kPeakReleaseDbPerS * dt);

            if (rm >= rmsDb)
                rmsDb += (rm - rmsDb) * (1.0f - std::exp (-dt / kRmsAttackSeconds));
            else
                rmsDb = std::max (rm, rmsDb - kRmsReleaseDbPerS * dt);

            if (peakDb >= holdDb)
            {
                holdDb = peakDb;
                holdTimer = kHoldSeconds;
            }
            else if (holdTimer > 0.0f)
            {
                holdTimer -= dt;
            }
            else
            {
                holdDb = std::max (peakDb, holdDb - kHoldFallDbPerS * dt);
            }

            if (clipped)
                clipTimer = kClipHoldSeconds;
            else
                clipTimer = std::max (0.0f, clipTimer - dt);
        }

        void resetClip() noexcept { clipTimer = 0.0f; }

        float getPeakDb() const noexcept { return peakDb; }
        float getRmsDb()  const noexcept { return rmsDb; }
        float getHoldDb() const noexcept { return holdDb; }
        bool  isClipLit() const noexcept { return clipTimer > 0.0f; }

    private:
        float peakDb = kFloorDb, rmsDb = kFloorDb, holdDb = kFloorDb;
        float holdTimer = 0.0f, clipTimer = 0.0f;
    };
} // namespace kb::engine
