#pragma once
// =============================================================================
//  Paleta oficial de KB - MUSIQUERO DJ (sección 6 del documento maestro).
//  Todos los colores del skin salen de acá: nunca usar colores sueltos.
// =============================================================================
#include <juce_graphics/juce_graphics.h>

namespace kb::ui::palette
{
    // Chasis
    inline const juce::Colour chassis       { 0xffF5D90A };  // amarillo
    inline const juce::Colour chassisLight  { 0xffFFE95C };
    inline const juce::Colour chassisShadow { 0xffB89F00 };

    // Paneles y mixer
    inline const juce::Colour panelDark     { 0xff121417 };
    inline const juce::Colour panel         { 0xff1A1D21 };
    inline const juce::Colour panelLight    { 0xff1E2126 };
    inline const juce::Colour panelEdge     { 0xff2C3037 };
    inline const juce::Colour slot          { 0xff0A0B0D };

    // Acentos
    inline const juce::Colour cyan          { 0xff19E3F5 };  // pads y botones de acento
    inline const juce::Colour orange        { 0xffFF8A00 };  // perillas, arcos, anillos
    inline const juce::Colour play          { 0xff2BEA5A };  // PLAY
    inline const juce::Colour alert         { 0xffFF4A3D };  // CUE / SYNC alerta

    // Waveforms
    inline const juce::Colour waveDeck1     = cyan;
    inline const juce::Colour waveDeck2     = orange;

    // Texto
    inline const juce::Colour textPrimary   { 0xffE8EAED };
    inline const juce::Colour textSecondary { 0xff8A9099 };
    inline const juce::Colour textOnChassis { 0xff1A1A1A };

    // Vúmetros (degradé verde -> amarillo -> naranja -> rojo)
    inline const juce::Colour meterGreen    { 0xff2BEA5A };
    inline const juce::Colour meterYellow   { 0xffF5D90A };
    inline const juce::Colour meterOrange   { 0xffFF8A00 };
    inline const juce::Colour meterRed      { 0xffFF3B30 };

    /** Color de cada deck (0 = deck 1, 1 = deck 2). */
    inline juce::Colour deckColour (int deckIndex) { return deckIndex == 0 ? waveDeck1 : waveDeck2; }
    /** Colores de los 8 hot cues (iguales en pads y waveform). */
    inline juce::Colour hotCueColour (int index)
    {
        static const juce::uint32 colours[8] = { 0xffFF4A3D, 0xffFF8A00, 0xffF5D90A, 0xff2BEA5A,
                                                 0xff19E3F5, 0xff3D7BFF, 0xffA85CFF, 0xffFF4FB4 };
        return juce::Colour (colours[(size_t) juce::jlimit (0, 7, index)]);
    }
} // namespace kb::ui::palette
