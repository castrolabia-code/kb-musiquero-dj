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
#include "ui/PlatterView.h"

#include <array>
#include <functional>

namespace kb::ui
{
    class ControllerView final : public juce::Component,
                                 public juce::TooltipClient
    {
    public:
        ControllerView();

        // --- Acciones del usuario -----------------------------------------------------
        std::function<void (int deck)>              onPlay;
        std::function<void (int deck)>              onPause;
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
        /** Tempo pedido con el fader, en % (+ = más rápido). */
        std::function<void (int deck, float percent)> onTempo;
        std::function<void (int deck)>              onSync;
        std::function<void (int deck)>              onKeyLock;
        /** delta = -1 / +1 semitono; 0 = volver al tono original. */
        std::function<void (int deck, int delta)>   onKeyShift;
        /** Botón AUTO MIX: empezar (o cancelar si ya está mezclando). */
        std::function<void()>                       onAutoMix;
        /** Duración de la mezcla automática elegida con la perilla MIX, en segundos. */
        std::function<void (double seconds)>        onAutoMixTime;

        // --- Performance (Fase 4) --------------------------------------------------------
        /** group: 0 HOT CUE, 1 FX, 2 SAMPLER, 3 LOOP. */
        std::function<void (int deck, int group)>             onPadMode;
        std::function<void (int deck, int pad, bool down)>    onPad;
        std::function<void (int deck, int pad, juce::Component* target)> onPadMenu;
        std::function<void (int deck, bool down)>             onShift;
        std::function<void (int deck)>                        onSlip;
        std::function<void (int deck)>                        onVinyl;
        std::function<void (int deck, bool touching)>         onJogTouch;
        std::function<void (int deck, double turns)>          onJogTurn;
        std::function<void (int deck, float delta)>           onJogWheel;

        struct PadView
        {
            int  group = 0;
            bool secondary = false;
            std::array<juce::String, 4> labels;
            std::array<juce::Colour, 4> colours;
            std::array<bool, 4>  lit {};
            std::array<float, 4> progress { -1.0f, -1.0f, -1.0f, -1.0f };
            bool shift = false, slip = false, vinyl = true;
        };
        void setPadView (int deck, const PadView& view);
        /** Pad debajo de un punto (coordenadas locales): devuelve el deck y escribe el pad, o -1. */
        int padAt (juce::Point<int> p, int& padIndex) const;

        // --- Estado a mostrar -------------------------------------------------------
        struct DeckLights
        {
            bool loaded     = false;
            bool playing    = false;
            bool cuePreview = false;
            bool atCue      = false;
        };
        /** blinkPhase alterna ~2 veces por segundo (PAUSA titila con la pista en pausa). */
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
        /** progress: 0..1 mientras mezcla, < 0 si no. */
        void setAutoMixState (float progress);
        void setAutoMixSeconds (double seconds);

        static constexpr double kAutoMixMinSeconds = 2.0, kAutoMixMaxSeconds = 60.0;
        static double knobToMixSeconds (double v01);
        static double mixSecondsToKnob (double seconds);

        /** Tempo actual en % (lo puede cambiar el SYNC). No pisa al usuario mientras arrastra. */
        void setTempo (int deck, float percent);
        void setSyncLit (int deck, bool on);
        void setKeyState (int deck, bool keyLock, int keyShift);
        /** Plato: llamar a cada cuadro (60 fps). */
        void setPlatter (int deck, double positionSeconds, bool playing, bool loaded);
        void setPlatterArtwork (int deck, const juce::Image& artwork);
        void setPlatterBeatGrid (int deck, double bpm, double firstBeatSeconds);
        /** Fuerza regenerar el fondo cacheado (p. ej. al cambiar el idioma). */
        void invalidateBackground();

        static constexpr float kTempoRanges[3] = { 8.0f, 16.0f, 50.0f };
        /** Rango actual del fader de tempo del deck, en % (8, 16 o 50). */
        float getTempoRange (int deck) const { return kTempoRanges[rangeIndex[(size_t) juce::jlimit (0, 1, deck)]]; }
        /** Pasa al rango siguiente (como el botón ±8 %). */
        void  cycleTempoRange (int deck);
        void  setPhonesFromEngine (float v) { setPhones (v); }

        /** Vúmetro de canal: llamar a cada cuadro (60 fps). */
        void pushChannelMeter (int deck, const engine::MeterSource::Reading& reading, float dtSeconds);

        /** Resalta el deck sobre el que se está arrastrando un archivo (-1 = ninguno). */
        void setDropTarget (int deck);
        /** Deck debajo de un punto (coordenadas locales), o -1. */
        int  deckAt (juce::Point<int> p) const;

        void paint (juce::Graphics& g) override;
        void resized() override;
        // Táctil (Fase 10): un toque entre controles va al más cercano.
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;

        /** Ayuda para lo que está dibujado en el fondo (platos, pads, etc.). */
        juce::String getTooltip() override;

    private:
        touch::NearestControl nearestTouch;
        struct DeckLayout
        {
            juce::Rectangle<float> area, tempoLabel, tempoSlot, tempoRange, header, platter, sync, cue, pause, play;
            juce::Rectangle<float> keyLock, keyDown, keyUp, shift, slip, vinyl;
            juce::Rectangle<float> number, chCue, chCueLabel;   // "(1)", CUE de auriculares del canal
            juce::Point<float> sideKnobCentre;                  // MASTER (deck 1) / HEADPHONES (deck 2)
            float sideKnobRadius = 8.0f;
            juce::Rectangle<float> artInner, artOuter, artHeader;   // zonas amarillas libres para los tribales
            std::array<juce::Rectangle<float>, 4> modes, pads;
            bool mirrored = false;
        };

        struct MixerLayout
        {
            juce::Rectangle<float> panel, title, crossfader, autoMix;
            juce::Point<float> mixTimeCentre;
            float mixTimeRadius = 6.0f;
            std::array<std::array<juce::Point<float>, 5>, 2> eqCentres {};
            float eqRadius = 8.0f;
            std::array<juce::Rectangle<float>, 2> cueButtons, faders;
            juce::Point<float> masterCentre, phonesCentre, cueMixCentre;
            float masterRadius = 10.0f, smallRadius = 8.0f;
            juce::Rectangle<float> meterL, meterR, split;
        };

        void computeLayout();
        void assignMidiIds();
        void renderBackground (float scale);
        void paintBackground (juce::Graphics& g) const;
        void paintDropOverlay (juce::Graphics& g) const;
        void updateTempoFader (int deck);
        void paintDeck (juce::Graphics& g, const DeckLayout& d, int deckIndex) const;
        void paintMixer (juce::Graphics& g) const;

        float unit = 1.0f;
        juce::Rectangle<float> chassis;
        std::array<DeckLayout, 2> deckLayouts;
        MixerLayout mixer;
        int dropTarget = -1;

        std::array<std::unique_ptr<GlowButton>, 2> playButtons, pauseButtons, cueButtons;
        std::array<std::unique_ptr<KbFader>, 2>    volumeFaders;
        KbFader crossfader { false };
        KbKnob  masterKnob { "mixer.master" };

        // Tira de canal: [0] GAIN, [1] HIGH, [2] MID, [3] LOW, [4] FILTER (de arriba hacia abajo)
        std::array<std::array<std::unique_ptr<KbKnob>, 5>, 2> stripKnobs;
        std::array<std::unique_ptr<GlowButton>, 2> channelCueButtons;
        std::array<std::unique_ptr<LevelMeter>, 2> channelMeters;
        KbKnob phonesKnob { "mixer.headphones" }, cueMixKnob { "mixer.cueMix" };
        GlowButton splitButton { "mixer.split", palette::orange, false };
        GlowButton autoMixButton { "mixer.autoMix", palette::cyan, false };
        KbKnob     mixTimeKnob { "mixer.mixTime" };

        // Tempo, SYNC y platos (Fase 3)
        std::array<std::unique_ptr<KbFader>, 2>     tempoFaders;
        std::array<std::unique_ptr<GlowButton>, 2>  syncButtons, rangeButtons;
        std::array<std::unique_ptr<GlowButton>, 2>  keyLockButtons, keyDownButtons, keyUpButtons;
        std::array<std::unique_ptr<PlatterView>, 2> platters;
        // Performance (Fase 4)
        std::array<std::array<std::unique_ptr<GlowButton>, 4>, 2> modeButtons, padButtons;
        std::array<std::unique_ptr<GlowButton>, 2> shiftButtons, slipButtons, vinylButtons;
        std::array<int, 2> padGroup { -1, -1 };
        std::array<bool, 2> padSecondary { false, false };

        std::array<int, 2>   rangeIndex { 0, 0 };
        std::array<float, 2> tempoPercent { 0.0f, 0.0f };

        // Fondo estático cacheado (chasis, mixer, etiquetas): repintar es solo copiar.
        juce::Image background;
        float backgroundScale = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControllerView)
    };
} // namespace kb::ui
