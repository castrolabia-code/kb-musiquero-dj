#pragma once
// =============================================================================
//  ChannelStrip: procesamiento de un canal del mixer (hilo de audio).
//
//      deck ─▶ GAIN/TRIM ─▶ EQ 3 bandas (isolator) ─▶ FILTER ─▶ [vúmetro, envío a CUE]
//
//  El fader de volumen y el crossfader se aplican después, en el motor, para
//  que el CUE de auriculares sea "pre-fader" como en cualquier mixer DJ.
//  Todos los parámetros se suavizan: se pueden mover sin "zipper noise".
// =============================================================================
#include "engine/MeterSource.h"
#include "engine/dsp/DjFilter.h"
#include "engine/dsp/ThreeBandIsolator.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace kb::engine
{
    class ChannelStrip
    {
    public:
        struct Params
        {
            float trim   = 0.5f;   // 0..1  (0.5 = 0 dB, rango ±12 dB)
            float low    = 0.5f;   // 0..1  (0 = kill, 0.5 = 0 dB, 1 = +6 dB)
            float mid    = 0.5f;
            float high   = 0.5f;
            float filter = 0.5f;   // 0..1  (LPF <- 0.5 -> HPF)
            float lowMultiplier = 1.0f;   // ganancia extra de graves (bass swap del AUTO MIX)
        };

        void prepare (double sampleRate) noexcept;

        /** Procesa en el lugar y alimenta el vúmetro del canal. */
        void process (float* left, float* right, int numSamples, const Params& params) noexcept;

        MeterSource& getMeter() noexcept { return meter; }

        /** Posición de la perilla GAIN (0..1) -> ganancia lineal (±12 dB). */
        static float trimKnobToGain (float position01) noexcept;

    private:
        dsp::ThreeBandIsolator eq[2];
        dsp::DjFilter filter;
        juce::SmoothedValue<float> trimGain, lowGain, midGain, highGain;
        MeterSource meter;
    };
} // namespace kb::engine
