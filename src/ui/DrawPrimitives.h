#pragma once
// =============================================================================
//  Primitivas de dibujo vectorial del skin (perillas, botones con glow,
//  faders, LEDs). Las usan los placeholders y los controles interactivos.
//  Todo se dibuja en coordenadas flotantes: escala sin pixelado en 4K.
// =============================================================================
#include <juce_graphics/juce_graphics.h>

namespace kb::ui::draw
{
    /** Ángulos del arco de las perillas (sentido horario desde las 12). */
    extern const float kArcStart;
    extern const float kArcEnd;

    void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float height,
               juce::Colour c, juce::Justification j = juce::Justification::centred, bool bold = true);

    /** Perilla con arco de valor naranja. value01 en [0, 1]. Etiqueta debajo.
        bipolar: el arco se dibuja desde el centro (perillas con 0 dB al medio). */
    void knob (juce::Graphics& g, juce::Point<float> centre, float radius, float value01,
               const juce::String& label, float textHeight, bool bipolar = false);

    /** Botón con glow. 'lit' = encendido. */
    void button (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, const juce::String& label,
                 bool lit, bool round, float textHeight);

    /** Ranura de fader con su tapa. pos01: 0 = abajo/izquierda, 1 = arriba/derecha. */
    void fader (juce::Graphics& g, juce::Rectangle<float> r, bool vertical, float pos01);

    /** Color de un segmento LED según su posición en la escala (0 = abajo). */
    juce::Colour meterColour (float t);

    /** Columna/fila de LEDs apagados (los vúmetros reales llegan en la Fase 2). */
    void meter (juce::Graphics& g, juce::Rectangle<float> r, bool vertical, int segments);
} // namespace kb::ui::draw
