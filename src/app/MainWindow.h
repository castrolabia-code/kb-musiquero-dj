#pragma once
// =============================================================================
//  MainWindow: ventana principal redimensionable, DPI-aware, con menú
//  Archivo / Opciones / Ayuda. Recuerda tamaño y posición entre sesiones.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/I18n.h"

namespace kb::app
{
    class AppSettings;
    class AudioDeviceService;
    class MainComponent;
    class PlayerController;
    class TrackLibrary;

    class MainWindow final : public juce::DocumentWindow,
                             private juce::MenuBarModel,
                             private I18n::Listener
    {
    public:
        MainWindow (AppSettings& settings, AudioDeviceService& audio, PlayerController& player, TrackLibrary& library);
        ~MainWindow() override;

        void closeButtonPressed() override;
        void resized() override;

        void showOptions (int tab);
        void showAbout();

        static constexpr int kMinWidth  = 1280;
        static constexpr int kMinHeight = 720;

    private:
        // juce::MenuBarModel
        juce::StringArray getMenuBarNames() override;
        juce::PopupMenu   getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
        void              menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

        // I18n::Listener
        void languageChanged() override;

        void setInitialBounds();

        AppSettings&        settings;
        AudioDeviceService& audio;
        PlayerController&   player;
        MainComponent*      mainComponent = nullptr;   // propiedad de la ventana
        float               menuScale = 1.0f;
        juce::Component::SafePointer<juce::DialogWindow> optionsWindow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };
} // namespace kb::app
