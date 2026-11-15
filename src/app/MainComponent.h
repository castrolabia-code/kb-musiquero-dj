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

#include "app/AppSettings.h"
#include "app/I18n.h"
#include "app/LibraryView.h"
#include "app/PlayerController.h"
#include "ui/ControllerView.h"
#include "ui/DeckDisplay.h"
#include "ui/SkinPlaceholders.h"
#include "ui/TopBar.h"

#include <array>
#include <memory>

namespace kb::app
{
    class AudioDeviceService;

    class MainComponent final : public juce::Component,
                                public juce::FileDragAndDropTarget,
                                public juce::DragAndDropContainer,
                                public juce::DragAndDropTarget,
                                private I18n::Listener,
                                private PlayerController::Listener,
                                private juce::Timer
    {
    public:
        MainComponent (AudioDeviceService& audio, PlayerController& player, TrackLibrary& library, AppSettings& settings);
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

        // juce::DragAndDropTarget (filas de la biblioteca -> decks)
        bool isInterestedInDragSource (const SourceDetails& details) override;
        void itemDragEnter (const SourceDetails& details) override;
        void itemDragMove (const SourceDetails& details) override;
        void itemDragExit (const SourceDetails& details) override;
        void itemDropped (const SourceDetails& details) override;

        /** Abre el menú "+" de la biblioteca. */
        void showAddToLibraryMenu() { libraryView.showAddMenu(); }

    private:
        void languageChanged() override;
        void trackLoaded (int deck) override;
        void trackLoadFailed (int deck, const juce::File& file, engine::LoadError error) override;
        void timerCallback() override;

        void updateStatusBar();
        void updateMeters();   // 60 fps, sincronizado con la pantalla
        ui::DeckDisplay::View makeDeckView (int deck) const;
        static constexpr int kLibraryTarget = -2;
        int  deckForDrop (int x, int y) const;
        /** Deck libre para cargar con doble click: el que no suena (el 1 si ninguno suena). */
        int  chooseFreeDeck() const;
        void setDropTarget (int deck);
        static juce::String describeError (engine::LoadError error);

        AudioDeviceService& audio;
        AppSettings& settings;
        double autoMixSeconds = 10.0;
        void toggleAutoMix();
        PlayerController&   player;

        ui::TopBar             topBar;
        ui::ControllerView     controller;
        ui::DeckDisplay        deckDisplays[2] { ui::DeckDisplay (0), ui::DeckDisplay (1) };
        TrackLibrary&          library;
        LibraryView            libraryView;
        ui::StatusBar          statusBar;

        std::unique_ptr<juce::FileChooser> chooser;
        juce::TooltipWindow tooltipWindow { this, 700 };   // ayuda al pasar el mouse (0,7 s)
        std::unique_ptr<juce::VBlankAttachment> vblank;
        double lastMeterTime = 0.0;

        // Marco amarillo redondeado con tribales alrededor de todo (cacheado).
        juce::Image frameImage;
        float frameScale = 0.0f;
        int frameThickness() const;
        void renderFrame (float scale);
        int dropTarget = -1;
        int tick = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
    };
} // namespace kb::app
