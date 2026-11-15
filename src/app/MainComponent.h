#pragma once
// =============================================================================
//  MainComponent: contenido de la ventana principal.
//  Layout (sección 6): barra superior / controlador / waveforms / biblioteca,
//  más una barra de estado con la información del dispositivo de audio.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/I18n.h"
#include "ui/SkinPlaceholders.h"

namespace kb::app
{
    class AudioDeviceService;

    class MainComponent final : public juce::Component,
                                private I18n::Listener,
                                private juce::Timer
    {
    public:
        explicit MainComponent (AudioDeviceService& audio);
        ~MainComponent() override;

        void paint (juce::Graphics& g) override;
        void resized() override;

    private:
        void languageChanged() override;
        void timerCallback() override;
        void updateStatus();

        AudioDeviceService& audio;

        ui::TopBarPlaceholder     topBar;
        ui::ControllerPlaceholder controller;
        ui::WaveformPlaceholder   waveforms;
        ui::LibraryPlaceholder    library;
        ui::StatusBar             statusBar;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
} // namespace kb::app
