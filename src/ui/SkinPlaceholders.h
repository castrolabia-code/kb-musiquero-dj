#pragma once
// =============================================================================
//  Placeholders del skin (Fase 0).
//
//  Dibujan por código, en vectorial, el MISMO layout que tendrá el skin final
//  (sección 6). Hoy solo queda la barra de estado; el resto ya es real. (El controlador y los displays de
//  los decks ya son interactivos: ui/ControllerView, ui/DeckDisplay y ui/TopBar.)
//  Todo se calcula en proporciones del área, así que escala sin pixelado en
//  1080p, 1440p y 4K.
//
//  No son funcionales: los controles reales llegan en las fases 1 a 7, y en la
//  Fase 7 el chasis y los platos pasan a renders PNG @1x/@2x/@4x.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace kb::ui
{
    /** Barra de estado inferior (dispositivo de audio, latencia, CPU). */
    class StatusBar final : public juce::Component
    {
    public:
        StatusBar();
        void setText (const juce::String& leftText, const juce::String& rightText);
        void paint (juce::Graphics& g) override;

    private:
        juce::String left, right;
    };
} // namespace kb::ui
