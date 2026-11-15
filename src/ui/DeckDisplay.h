#pragma once
// =============================================================================
//  DeckDisplay: franja de un deck en la zona de waveforms (sección 5.7).
//
//    [carátula] título / artista / BPM / KEY / tiempos | waveform ampliada (zoom)
//                                                      | vista general (click = saltar)
//
//  * Waveform por frecuencia: graves (color del deck oscuro), medios (color del
//    deck) y agudos (casi blanco). Cyan en el deck 1 y naranja en el deck 2.
//  * Vista ampliada que avanza con la canción, playhead al centro, grilla de
//    beats y marca de cue. Rueda del mouse = zoom (2 a 32 segundos).
//  * Vista general de toda la pista: parte reproducida atenuada, cue y click /
//    arrastre para saltar.
//  Las waveforms se cachean en imágenes: a 60 fps solo se copian.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Deck.h"
#include "engine/TrackBuffer.h"

#include <functional>
#include <memory>

namespace kb::ui
{
    class DeckDisplay final : public juce::Component,
                              public juce::TooltipClient
    {
    public:
        explicit DeckDisplay (int deckIndex);

        struct View
        {
            std::shared_ptr<engine::TrackBuffer> track;
            engine::Deck::Status status;
            bool  loading   = false;
            bool  analysing = false;
            bool  keyLock   = false;
            float tempo     = 0.0f;   // %
            int   keyShift  = 0;      // semitonos
        };

        /** El usuario hizo click en la vista general: posición pedida, en segundos. */
        std::function<void (double seconds)> onSeek;

        /** Llamar a cada cuadro (60 fps). Repinta solo si algo cambió. */
        void update (const View& view);

        void setDropHighlight (bool shouldHighlight);
        void refresh();

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        juce::String getTooltip() override;

        /** "01:23.4" (o "-01:23.4" si negative). */
        static juce::String formatTime (double seconds, bool negative = false);
        /** "128.00" */
        static juce::String formatBpm (double bpm);
        /** Semitonos efectivos de corrimiento (key shift + cambio de tono por tempo si no hay Key Lock). */
        static int effectiveShift (float tempoPercent, bool keyLock, int keyShift);

    private:
        juce::Rectangle<float> infoArea() const;
        juce::Rectangle<float> detailArea() const;
        juce::Rectangle<float> overviewArea() const;

        void paintInfo (juce::Graphics& g, juce::Rectangle<float> area);
        void paintDetail (juce::Graphics& g, juce::Rectangle<float> area, float scale);
        void paintOverview (juce::Graphics& g, juce::Rectangle<float> area, float scale);
        void renderDetailCache (juce::Rectangle<float> area, float scale, double startSeconds);
        void renderOverviewCache (juce::Rectangle<float> area, float scale);
        void seekFromMouse (const juce::MouseEvent& e);

        /** Colores de las 3 bandas para este deck. */
        juce::Colour bandColour (int band) const;

        int  deck;
        View view;
        bool dropHighlight = false;
        double zoomSeconds = 8.0;

        // Caché de la vista ampliada: cubre 3 ventanas de zoom alrededor del playhead.
        juce::Image   detailImage;
        double        detailStart = 0.0;
        double        detailZoom  = 0.0;
        std::uint64_t detailTrack = 0;
        int           detailBins  = -1;
        float         detailScale = 0.0f;
        int           detailWidth = 0;

        // Caché de la vista general
        juce::Image   overviewImage;
        std::uint64_t overviewTrack = 0;
        int           overviewBins  = -1;
        float         overviewScale = 0.0f;
        int           overviewWidth = 0;

        // Para repintar solo cuando cambia algo visible
        juce::String lastSignature;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckDisplay)
    };
} // namespace kb::ui
