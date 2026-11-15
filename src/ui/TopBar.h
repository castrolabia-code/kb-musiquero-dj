#pragma once
// =============================================================================
//  TopBar: barra superior (sección 6, punto 1).
//    "KB MUSIQUERO DJ"   [vúmetro master L / R con escala dBFS]  LIMIT   BY CASTRO LABS
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/LevelMeter.h"

namespace kb::ui
{
    class TopBar final : public juce::Component
    {
    public:
        TopBar();

        /** Llamar a cada cuadro con las lecturas del master y la reducción del limitador. */
        void pushMaster (const engine::MeterSource::Reading& left, const engine::MeterSource::Reading& right,
                         float limiterReductionDb, float dtSeconds);

        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        juce::Rectangle<float> meterArea, scaleArea, limitArea;
        LevelMeter meterL { false, 40 }, meterR { false, 40 };
        bool limiting = false;
        float limitHold = 0.0f;
    };
} // namespace kb::ui
