#pragma once
// =============================================================================
//  PlatterView: el disco de cada bandeja. Gira a 33⅓ rpm siguiendo la
//  posición real de la pista (si se acelera el tempo, gira más rápido; en
//  pausa queda quieto). El anillo naranja se enciende mientras reproduce.
//
//  Solo dibuja lo que se mueve; la base metálica y su sombra están en el fondo
//  cacheado de ControllerView, así repintar a 60 fps es barato.
//  Fase 4: además recibe el mouse para scratch / pitch bend.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "library/ImageResize.h"

#include <functional>

namespace kb::ui
{
    class PlatterView final : public juce::Component,
                              public juce::TooltipClient
    {
    public:
        static constexpr double kRevolutionsPerSecond = (100.0 / 3.0) / 60.0;   // 33⅓ rpm

        explicit PlatterView (int deckIndex);

        /** Ángulo a partir de la posición de la pista (segundos). */
        static double angleForPosition (double seconds) noexcept;

        /** Llamar a cada cuadro. Repinta solo si algo cambió visiblemente. */
        void setState (double positionSeconds, bool playing, bool loaded);
        /** Carátula de la canción: se dibuja en el centro del disco y gira con él. */
        void setArtwork (const juce::Image& image);
        /** Grilla de la pista: el aro late con cada beat mientras suena. */
        void setBeatGrid (double bpm, double firstBeatSeconds) { beatBpm = bpm; beatFirst = firstBeatSeconds; }

        void paint (juce::Graphics& g) override;

        // Jog (Fase 4): tocar/soltar, giro en vueltas (+ horario) y rueda del mouse.
        std::function<void (bool touching)>  onTouch;
        std::function<void (double turns)>   onTurn;
        std::function<void (float delta)>    onWheel;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        bool hitTest (int x, int y) override;
        juce::String getTooltip() override;

    private:
        double angleAt (juce::Point<float> p) const;
        double lastAngle = 0.0;
        bool grabbed = false;
        juce::Image artwork;
        kb::library::ScaledImageCache labelCache;   // carátula al tamaño real, en círculo
        double beatBpm = 0.0, beatFirst = 0.0, position = 0.0;
        int deck;
        double angle = 0.0;
        bool playing = false, loaded = false;
    };
} // namespace kb::ui
