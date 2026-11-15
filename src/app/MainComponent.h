#pragma once
// =============================================================================
//  MainComponent: contenido de la ventana principal.
//  Layout (sección 6): barra superior / controlador / waveforms / biblioteca,
//  más una barra de estado con la información del dispositivo de audio.
//
//  Fase 1: conecta la UI con PlayerController (transporte, mixer básico,
//  arrastrar y soltar archivos desde el Explorador sobre cada deck).
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/I18n.h"
#include "app/PlayerController.h"
#include "ui/ControllerView.h"
#include "ui/DeckDisplay.h"
#include "ui/SkinPlaceholders.h"

#include <array>
#include <memory>

namespace kb::app
{
    class AudioDeviceService;

    class MainComponent final : public juce::Component,
                                public juce::FileDragAndDropTarget,
                                private I18n::Listener,
                                private PlayerController::Listener,
                                private juce::Timer
    {
    public:
        MainComponent (AudioDeviceService& audio, PlayerController& player);
        ~MainComponent() override;

        /** Abre el diálogo para elegir un archivo y cargarlo en un deck. */
        void openLoadDialog (int deck);

        void paint (juce::Graphics& g) override;
        void resized() override;

        // juce::FileDragAndDropTarget
        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void fileDragEnter (const juce::StringArray& files, int x, int y) override;
        void fileDragMove (const juce::StringArray& files, int x, int y) override;
        void fileDragExit (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;

    private:
        void languageChanged() override;
        void trackLoaded (int deck) override;
        void trackLoadFailed (int deck, const juce::File& file, engine::LoadError error) override;
        void timerCallback() override;

        void updateStatusBar();
        int  deckForDrop (int x, int y) const;
        void setDropTarget (int deck);
        static juce::String describeError (engine::LoadError error);

        AudioDeviceService& audio;
        PlayerController&   player;

        ui::TopBarPlaceholder  topBar;
        ui::ControllerView     controller;
        ui::DeckDisplay        deckDisplays[2] { ui::DeckDisplay (0), ui::DeckDisplay (1) };
        ui::LibraryPlaceholder library;
        ui::StatusBar          statusBar;

        std::unique_ptr<juce::FileChooser> chooser;
        int dropTarget = -1;
        int tick = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
} // namespace kb::app
