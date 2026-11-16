#pragma once
// =============================================================================
//  OptionsComponent: ventana de Opciones con pestañas.
//    * Audio:   tipo de driver (WASAPI compartido / exclusivo / ASIO),
//               dispositivo, sample rate, buffer y salidas activas (hasta 4).
//    * Mixer:   curva e inversión del crossfader, ruteo de salidas y SPLIT.
//    * General: idioma de la interfaz (se aplica en vivo).
// =============================================================================
#include <juce_audio_utils/juce_audio_utils.h>

#include "app/I18n.h"
#include "engine/AudioEngine.h"

namespace kb::app
{
    class AppSettings;
    class AudioDeviceService;
    class MidiController;
    class Keymap;

    class OptionsComponent final : public juce::Component,
                                   private I18n::Listener
    {
    public:
        enum Tab { audioTab = 0, mixerTab = 1, generalTab = 2, midiTab = 3, keyboardTab = 4 };

        OptionsComponent (AudioDeviceService& audio, AppSettings& settings, engine::AudioEngine& engine,
                          MidiController* midi, Keymap* keymap, std::function<void()> keymapChanged, Tab initialTab);
        ~OptionsComponent() override;

        void showTab (Tab tab);
        void resized() override;

        /** Título traducido de la ventana. */
        static juce::String getWindowTitle();

    private:
        class AudioPage;
        class MixerPage;
        class GeneralPage;
        class MidiPage;
        class KeyboardPage;

        void languageChanged() override;

        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        AudioPage*   audioPage   = nullptr;   // propiedad de 'tabs'
        MixerPage*   mixerPage   = nullptr;   // propiedad de 'tabs'
        GeneralPage* generalPage = nullptr;   // propiedad de 'tabs'
        MidiPage*    midiPage    = nullptr;   // propiedad de 'tabs' (si hay MIDI)
        KeyboardPage* keyboardPage = nullptr; // propiedad de 'tabs'

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionsComponent)
    };
} // namespace kb::app
