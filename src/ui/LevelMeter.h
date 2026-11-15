#pragma once
// =============================================================================
//  LevelMeter: vúmetro LED digital (sección 5.5).
//
//    * Barra de RMS brillante + extensión hasta el pico más tenue.
//    * Segmento de retención de pico y LED de CLIP (click para apagarlo).
//    * Escala en dBFS no lineal (más resolución cerca de 0 dB).
//    * Degradé verde -> amarillo -> naranja -> rojo, con glow.
//  La balística vive en engine::MeterBallistics; acá solo se dibuja.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/MeterBallistics.h"
#include "engine/MeterSource.h"

#include <vector>

namespace kb::ui
{
    class LevelMeter final : public juce::Component,
                             public juce::TooltipClient
    {
    public:
        LevelMeter (bool vertical, int numSegments);

        void setTooltipKey (const juce::String& key) { tooltipKey = key; }
        juce::String getTooltip() override;

        /** Llamar a cada cuadro (60 fps) con la lectura del motor. */
        void push (const engine::MeterSource::Reading& reading, float dtSeconds);

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        /** dBFS -> posición 0..1 en la escala del vúmetro. */
        static float dbToPosition (float db) noexcept;
        /** Marcas de escala recomendadas (para etiquetar). */
        static const std::vector<float>& scaleMarks();

    private:
        juce::Colour segmentColour (float dbTop) const noexcept;

        bool vertical;
        int  numSegments;
        juce::String tooltipKey;
        std::vector<float> segmentDb;   // dB del borde superior de cada segmento
        engine::MeterBallistics ballistics;

        int litRms = -1, litPeak = -1, litHold = -1;
        bool clipLit = false;
    };
} // namespace kb::ui
