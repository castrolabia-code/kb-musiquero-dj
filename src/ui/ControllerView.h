#pragma once
// =============================================================================
//  ControllerView: cuerpo del controlador (DECK 1 / MIXER / DECK 2 en espejo)
//  sobre el chasis amarillo.
//
//  Fase 1 — funcionales: PLAY, CUE, fader de volumen por canal, crossfader y
//  MASTER. El resto (tempo, platos, pads, SYNC, EQ, filtro, auriculares,
//  vúmetros) se dibuja en su lugar definitivo y se activa en fases siguientes.
//
//  La vista no conoce al motor: expone callbacks y setters de estado.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Controls.h"

#include <array>
#include <functional>

namespace kb::ui
{
    class ControllerView final : public juce::Component
    {
    public:
        ControllerView();

        // --- Acciones del usuario -----------------------------------------------------
        std::function<void (int deck)>              onPlay;
        std::function<void (int deck)>              onCueDown;
        std::function<void (int deck)>              onCueUp;
        std::function<void (int deck, float value)> onVolume;
        std::function<void (float value)>           onCrossfader;   // -1..1
        std::function<void (float value)>           onMaster;       // 0..1

        // --- Estado a mostrar -------------------------------------------------------
        struct DeckLights
        {
            bool loaded     = false;
            bool playing    = false;
            bool cuePreview = false;
            bool atCue      = false;
        };
        /** blinkPhase alterna ~2 veces por segundo (PLAY titila en pausa, como un CDJ). */
        void setDeckLights (int deck, const DeckLights& lights, bool blinkPhase);

        void setVolume (int deck, float value);
        void setCrossfader (float value);
        void setMaster (float value);

        /** Resalta el deck sobre el que se está arrastrando un archivo (-1 = ninguno). */
        void setDropTarget (int deck);
        /** Deck debajo de un punto (coordenadas locales), o -1. */
        int  deckAt (juce::Point<int> p) const;

        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        struct DeckLayout
        {
            juce::Rectangle<float> area, tempoLabel, tempoSlot, header, platter, sync, cue, play;
            std::array<juce::Rectangle<float>, 4> modes, pads;
            bool mirrored = false;
        };

        struct MixerLayout
        {
            juce::Rectangle<float> panel, title, crossfader;
            std::array<std::array<juce::Point<float>, 5>, 2> eqCentres {};
            float eqRadius = 8.0f;
            std::array<juce::Rectangle<float>, 2> cueButtons, faders;
            juce::Point<float> masterCentre, phonesCentre, cueMixCentre;
            float masterRadius = 10.0f, smallRadius = 8.0f;
            juce::Rectangle<float> meterL, meterR;
        };

        void computeLayout();
        void paintDeck (juce::Graphics& g, const DeckLayout& d, int deckIndex) const;
        void paintMixer (juce::Graphics& g) const;

        float unit = 1.0f;
        juce::Rectangle<float> chassis;
        std::array<DeckLayout, 2> deckLayouts;
        MixerLayout mixer;
        int dropTarget = -1;

        std::array<std::unique_ptr<GlowButton>, 2> playButtons, cueButtons;
        std::array<std::unique_ptr<KbFader>, 2>    volumeFaders;
        KbFader crossfader { false };
        KbKnob  masterKnob { "mixer.master" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControllerView)
    };
} // namespace kb::ui
