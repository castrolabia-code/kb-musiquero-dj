#pragma once
// =============================================================================
//  Placeholders del skin (Fase 0).
//
//  Dibujan por código, en vectorial, el MISMO layout que tendrá el skin final
//  (sección 6): barra superior, controlador (deck 1 / mixer / deck 2),
//  waveforms y biblioteca. Todo se calcula en proporciones del área, así que
//  escala sin pixelado en 1080p, 1440p y 4K.
//
//  No son funcionales: los controles reales llegan en las fases 1 a 7, y en la
//  Fase 7 el chasis y los platos pasan a renders PNG @1x/@2x/@4x.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace kb::ui
{
    /** Barra superior: logo, vúmetro master (apagado) y "BY CASTRO LABS". */
    class TopBarPlaceholder final : public juce::Component
    {
    public:
        TopBarPlaceholder();
        void paint (juce::Graphics& g) override;
    };

    /** Cuerpo del controlador: DECK 1, MIXER y DECK 2 en espejo sobre el chasis amarillo. */
    class ControllerPlaceholder final : public juce::Component
    {
    public:
        ControllerPlaceholder();
        void paint (juce::Graphics& g) override;

    private:
        void paintDeck  (juce::Graphics& g, juce::Rectangle<float> area, int deckIndex, bool mirrored, float unit) const;
        void paintMixer (juce::Graphics& g, juce::Rectangle<float> area, float unit) const;
    };

    /** Zona de waveforms: una franja por deck con su info (carátula, BPM, key, tiempos). */
    class WaveformPlaceholder final : public juce::Component
    {
    public:
        WaveformPlaceholder();
        void paint (juce::Graphics& g) override;
    };

    /** Biblioteca: árbol de carpetas/playlists a la izquierda y columnas de pistas. */
    class LibraryPlaceholder final : public juce::Component
    {
    public:
        LibraryPlaceholder();
        void paint (juce::Graphics& g) override;
    };

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
