#pragma once
// =============================================================================
//  Keymap (Fase 9): atajos de teclado configurables. Una tecla dispara una
//  función de botón (las mismas que el MIDI: "deck1.playPause", "deck2.cue"...).
//  Mantener apretada la tecla = mantener apretado el botón (CUE, pads).
//
//  Predeterminados: Q/W PLAY-PAUSA, A/S CUE, Z/X SYNC, 1-4 pads deck 1,
//  7-0 pads deck 2, E/R KEY LOCK, Ctrl+B biblioteca grande.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include <map>

namespace kb::app
{
    class Keymap final
    {
    public:
        Keymap() { resetToDefaults(); }

        void resetToDefaults();
        /** Función de una tecla (vacío si no tiene). */
        juce::String targetFor (const juce::KeyPress& key) const;
        /** Tecla de una función (inválida si no tiene). */
        juce::KeyPress keyFor (const juce::String& target) const;
        /** Asigna (una tecla hace una sola cosa y cada función tiene una sola tecla). */
        void assign (const juce::KeyPress& key, const juce::String& target);
        void removeTarget (const juce::String& target);
        const std::map<juce::String, juce::String>& entries() const noexcept { return keys; }   // tecla (texto) -> función

        juce::String toJson() const;
        bool fromJson (const juce::String& json);

    private:
        static juce::String keyText (const juce::KeyPress& k);
        std::map<juce::String, juce::String> keys;
    };
} // namespace kb::app
