#pragma once
// =============================================================================
//  TribalArt: ornamentos tribales tipo grafiti, generados por código (diseño
//  propio: espirales "koru", espinas curvas y bandas simétricas).
//  Devuelven Paths vectoriales: se ven nítidos en cualquier resolución.
// =============================================================================
#include <juce_graphics/juce_graphics.h>

namespace kb::ui::tribal
{
    /** Espiral que se afina hacia el centro. dir = +1 horario, -1 antihorario. */
    juce::Path koru (juce::Point<float> centre, float radius, float turns, float startAngle, float dir, float thickness);

    /** Espina curva: de una base ancha a una punta fina. bend > 0 curva hacia un lado. */
    juce::Path thorn (juce::Point<float> baseA, juce::Point<float> baseB, juce::Point<float> tip, float bend);

    /** Banda horizontal simétrica (como un brazalete tribal) que llena el rectángulo. */
    juce::Path band (juce::Rectangle<float> area);

    /** Ornamento vertical simétrico (llama + espirales) para zonas altas y angostas. */
    juce::Path flame (juce::Rectangle<float> area);

    /** Esquinero: espiral + espinas que siguen los dos bordes. quadrant: 0 sup-izq, 1 sup-der, 2 inf-der, 3 inf-izq. */
    juce::Path corner (juce::Rectangle<float> area, int quadrant);

    /** Trama de fondo (espirales y espinas repetidas) para usar con poca opacidad. */
    juce::Path texture (juce::Rectangle<float> area, float cell);
} // namespace kb::ui::tribal
