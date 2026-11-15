#pragma once
// =============================================================================
//  ControllerView: cuerpo del controlador (DECK 1 / MIXER / DECK 2 en espejo)
//  sobre el chasis amarillo.
//
//  Funcionales: PLAY, CUE, faders de canal, crossfader, MASTER (Fase 1) y
//  GAIN, EQ 3 bandas, FILTER, CUE de canal, PHONES, CUE MIX, SPLIT y
//  vúmetros de canal (Fase 2). El resto (tempo, platos, pads, SYNC) se dibuja
//  en su lugar definitivo y se activa en fases siguientes.
//
//  La vista no conoce al motor: expone callbacks y setters de estado.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Controls.h"
#include "ui/LevelMeter.h"
#include "ui/Palette.h"

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
        std::function<void (int deck, float value)> onTrim;
        /** band: 0 = graves, 1 = medios, 2 = agudos. */
        std::function<void (int deck, int band, float value)> onEq;
        std::function<void (int deck, float value)> onFilter;
        std::function<void (int deck, bool on)>     onChannelCue;
        std::function<void (float value)>           onPhones;
        std::function<void (float value)>           onCueMix;
        std::function<void (bool on)>               onSplit;

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
        void setTrim (int deck, float value);
        void setEq (int deck, int band, float value);
        void setFilter (int deck, float value);
        void setChannelCue (int deck, bool on);
        void setPhones (float value);
        void setCueMix (float value);
        void setSplit (bool on);

        /** Vúmetro de canal: llamar a cada cuadro (60 fps). */
        void pushChannelMeter (int deck, const engine::MeterSource::Reading& reading, float dtSeconds);

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
            juce::Rectangle<float> meterL, meterR, split;
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

        // Tira de canal: [0] GAIN, [1] HIGH, [2] MID, [3] LOW, [4] FILTER (de arriba hacia abajo)
        std::array<std::array<std::unique_ptr<KbKnob>, 5>, 2> stripKnobs;
        std::array<std::unique_ptr<GlowButton>, 2> channelCueButtons;
        std::array<std::unique_ptr<LevelMeter>, 2> channelMeters;
        KbKnob phonesKnob { "mixer.headphones" }, cueMixKnob { "mixer.cueMix" };
        GlowButton splitButton { "mixer.split", palette::orange, false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControllerView)
    };
} // namespace kb::ui
