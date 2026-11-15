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

namespace kb::ui
{
    class PlatterView final : public juce::Component
    {
    public:
        static constexpr double kRevolutionsPerSecond = (100.0 / 3.0) / 60.0;   // 33⅓ rpm

        explicit PlatterView (int deckIndex);

        /** Ángulo a partir de la posición de la pista (segundos). */
        static double angleForPosition (double seconds) noexcept;

        /** Llamar a cada cuadro. Repinta solo si algo cambió visiblemente. */
        void setState (double positionSeconds, bool playing, bool loaded);

        void paint (juce::Graphics& g) override;

    private:
        int deck;
        double angle = 0.0;
        bool playing = false, loaded = false;
    };
} // namespace kb::ui
