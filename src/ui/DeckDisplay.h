#pragma once
// =============================================================================
//  DeckDisplay: franja de un deck en la zona de waveforms.
//
//    [carátula] título / artista / BPM-KEY / tiempos  |  vista de la pista
//
//  Fase 1: vista general de picos de toda la pista (se va dibujando mientras
//  carga), parte reproducida atenuada, marca de cue y playhead. Click o
//  arrastre sobre la vista = saltar a esa posición.
//  Fase 3: waveform RGB por frecuencia, vista ampliada y beatgrid (OpenGL).
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "engine/Deck.h"
#include "engine/TrackBuffer.h"

#include <functional>
#include <memory>

namespace kb::ui
{
    class DeckDisplay final : public juce::Component
    {
    public:
        explicit DeckDisplay (int deckIndex);

        /** El usuario hizo click en la vista: posición pedida, en segundos. */
        std::function<void (double seconds)> onSeek;

        /** Llamar ~30 veces por segundo desde la UI. Repinta solo si algo cambió. */
        void update (std::shared_ptr<engine::TrackBuffer> track, const engine::Deck::Status& status, bool loading,
                     float tempoPercent, bool analysing);

        /** "128.00" (BPM efectivo con el tempo aplicado). */
        static juce::String formatBpm (double bpm);

        void setDropHighlight (bool shouldHighlight);
        /** Fuerza repintar (p. ej. al cambiar el idioma). */
        void refresh();

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;

        /** "01:23.4" (o "-01:23.4" si negative). */
        static juce::String formatTime (double seconds, bool negative = false);

    private:
        void seekFromMouse (const juce::MouseEvent& e);
        void renderOverview (float scale);
        juce::Rectangle<float> waveArea() const;

        int deck;
        std::shared_ptr<engine::TrackBuffer> track;
        engine::Deck::Status status;
        bool loading = false;
        bool analysing = false;
        float tempo = 0.0f;
        double shownBpm = -1.0;
        bool dropHighlight = false;

        juce::Image overview;           // caché de la vista general
        std::uint64_t overviewTrackId = 0;
        int   overviewBins  = -1;
        float overviewScale = 0.0f;

        // Para repintar solo cuando cambia algo visible
        int lastTenths = -1;
        int lastPlayheadPx = -1;
        float lastProgress = -1.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeckDisplay)
    };
} // namespace kb::ui
