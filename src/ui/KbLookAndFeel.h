#pragma once
// =============================================================================
//  KbLookAndFeel: estilo base oscuro para diálogos, menús y controles estándar.
//  En la Fase 7 se extiende con el skin híbrido completo.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace kb::ui
{
    class KbLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        KbLookAndFeel();

        /** Fuente de títulos/etiquetas. Fase 7: Orbitron/Rajdhani embebidas (OFL). */
        static juce::Font labelFont (float height, bool bold = true);
    };
} // namespace kb::ui
