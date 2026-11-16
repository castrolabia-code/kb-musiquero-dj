#pragma once
// =============================================================================
//  TopBar: barra superior (sección 6, punto 1).
//    "KB MUSIQUERO DJ"   [vúmetro master L / R con escala dBFS]  LIMIT   BY CASTRO LABS
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Controls.h"
#include "ui/LevelMeter.h"

#include <functional>

namespace kb::ui
{
    class TopBar final : public juce::Component,
                         public juce::TooltipClient
    {
    public:
        TopBar();

        /** Llamar a cada cuadro con las lecturas del master y la reducción del limitador. */
        void pushMaster (const engine::MeterSource::Reading& left, const engine::MeterSource::Reading& right,
                         float limiterReductionDb, float dtSeconds);

        void paint (juce::Graphics& g) override;
        juce::String getTooltip() override;

        /** Botón de la vista "Biblioteca grande". */
        std::function<void()> onLibraryView;
        void setLibraryViewLit (bool on) { libraryButton.setLit (on); }
        void resized() override;

    private:
        juce::Rectangle<float> meterArea, scaleArea, limitArea, boxArea;
        LevelMeter meterL { false, 40 }, meterR { false, 40 };
        GlowButton libraryButton { "view.largeLibrary", juce::Colour (0xff19E3F5), false };
        bool limiting = false;
        float limitHold = 0.0f;

        // Logo neón 3D cacheado (se regenera al cambiar el tamaño).
        juce::Image logo;
        float logoScale = 0.0f;
        juce::Rectangle<float> logoArea, brandArea;
        void renderLogo (float scale);
    };
} // namespace kb::ui
