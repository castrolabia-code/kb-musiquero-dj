#pragma once
// =============================================================================
//  OptionsComponent: ventana de Opciones con pestañas.
//    * Audio:   tipo de driver (WASAPI compartido / exclusivo / ASIO),
//               dispositivo, sample rate, buffer y salidas activas (hasta 4).
//    * General: idioma de la interfaz (se aplica en vivo).
// =============================================================================
#include <juce_audio_utils/juce_audio_utils.h>

#include "app/I18n.h"

namespace kb::app
{
    class AppSettings;
    class AudioDeviceService;

    class OptionsComponent final : public juce::Component,
                                   private I18n::Listener
    {
    public:
        enum Tab { audioTab = 0, generalTab = 1 };

        OptionsComponent (AudioDeviceService& audio, AppSettings& settings, Tab initialTab);
        ~OptionsComponent() override;

        void showTab (Tab tab);
        void resized() override;

        /** Título traducido de la ventana. */
        static juce::String getWindowTitle();

    private:
        class AudioPage;
        class GeneralPage;

        void languageChanged() override;

        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        AudioPage*   audioPage   = nullptr;   // propiedad de 'tabs'
        GeneralPage* generalPage = nullptr;   // propiedad de 'tabs'

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionsComponent)
    };
} // namespace kb::app
