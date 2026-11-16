#include "app/MainWindow.h"

#include "ui/KbLookAndFeel.h"

#include "app/AppSettings.h"
#include "app/AudioDeviceService.h"
#include "app/MainComponent.h"
#include "app/OptionsComponent.h"
#include "app/PlayerController.h"
#include "ui/Palette.h"

namespace kb::app
{
    namespace
    {
        enum MenuIds
        {
            idExit = 1,
            idLoadDeck1,
            idLoadDeck2,
            idAddToLibrary,
            idOptionsAudio,
            idOptionsMixer,
            idOptionsGeneral,
            idLanguageBase = 100,   // + índice del idioma
            idAbout = 200,
            idViewLargeLibrary = 300
        };
    }

    MainWindow::MainWindow (AppSettings& s, AudioDeviceService& a, PlayerController& p, TrackLibrary& library)
        : juce::DocumentWindow (tr ("app.title"), ui::palette::panelDark, juce::DocumentWindow::allButtons),
          settings (s),
          audio (a),
          player (p)
    {
        setUsingNativeTitleBar (true);
        mainComponent = new MainComponent (audio, player, library, settings);
        setContentOwned (mainComponent, false);
        setMenuBar (this);

        setResizable (true, false);
        setResizeLimits (kMinWidth, kMinHeight, 16384, 16384);
        setInitialBounds();

        I18n::getInstance().addListener (this);
        setVisible (true);
    }

    MainWindow::~MainWindow()
    {
        I18n::getInstance().removeListener (this);
        settings.setMainWindowState (getWindowStateAsString());

        if (optionsWindow != nullptr)
            delete optionsWindow.getComponent();

        setMenuBar (nullptr);
        clearContentComponent();
    }

    void MainWindow::setInitialBounds()
    {
        const auto saved = settings.getMainWindowState();
        if (saved.isNotEmpty() && restoreWindowStateFromString (saved))
        {
            // Nunca restaurar por debajo del mínimo (p. ej. si cambió el monitor).
            if (getWidth() < kMinWidth || getHeight() < kMinHeight)
                setSize (juce::jmax (getWidth(), kMinWidth), juce::jmax (getHeight(), kMinHeight));
            return;
        }

        // Primera vez: 85 % del área útil de la pantalla principal (mínimo 1280x720).
        juce::Rectangle<int> area { 0, 0, 1600, 900 };
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            area = display->userBounds.toNearestInt();

        const auto w = juce::jmax (kMinWidth,  juce::roundToInt (area.getWidth()  * 0.85));
        const auto h = juce::jmax (kMinHeight, juce::roundToInt (area.getHeight() * 0.85));
        centreWithSize (w, h);
    }

    void MainWindow::closeButtonPressed()
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void MainWindow::resized()
    {
        // Pantallas grandes: la barra de menú crece con la escala de la interfaz.
        const auto scale = juce::jlimit (1.0f, 4.0f, (float) getHeight() / 1080.0f);
        ui::KbLookAndFeel::setUiScale (scale);
        if (std::abs (scale - menuScale) > 0.05f)
        {
            menuScale = scale;
            setMenuBar (this, getLookAndFeel().getDefaultMenuBarHeight());
        }
        juce::DocumentWindow::resized();
    }

    // --- Menú ---------------------------------------------------------------------
    juce::StringArray MainWindow::getMenuBarNames()
    {
        return { tr ("menu.file"), tr ("menu.view"), tr ("menu.options"), tr ("menu.help") };
    }

    juce::PopupMenu MainWindow::getMenuForIndex (int topLevelMenuIndex, const juce::String&)
    {
        juce::PopupMenu menu;
        switch (topLevelMenuIndex)
        {
            case 0:
                menu.addItem (idLoadDeck1, tr ("menu.file.loadDeck", { { "n", "1" } }));
                menu.addItem (idLoadDeck2, tr ("menu.file.loadDeck", { { "n", "2" } }));
                menu.addItem (idAddToLibrary, tr ("menu.file.addToLibrary"));
                menu.addSeparator();
                menu.addItem (idExit, tr ("menu.file.exit"));
                break;

            case 1:
            {
                juce::PopupMenu::Item item (tr ("view.largeLibraryMenu"));
                item.itemID = idViewLargeLibrary;
                item.isTicked = mainComponent != nullptr && mainComponent->isLargeLibrary();
                item.shortcutKeyDescription = "Ctrl+B";
                menu.addItem (item);
                break;
            }

            case 2:
            {
                menu.addItem (idOptionsAudio, tr ("menu.options.audio"));
                menu.addItem (idOptionsMixer, tr ("menu.options.mixer"));
                menu.addItem (idOptionsGeneral, tr ("menu.options.general"));
                menu.addSeparator();

                juce::PopupMenu languages;
                int i = 0;
                for (auto lang : I18n::allLanguages())
                {
                    languages.addItem (idLanguageBase + i, tr ("language." + I18n::toCode (lang)), true,
                                       lang == I18n::getInstance().getLanguage());
                    ++i;
                }
                menu.addSubMenu (tr ("menu.options.language"), languages);
                break;
            }

            case 3:
                menu.addItem (idAbout, tr ("menu.help.about"));
                break;

            default:
                break;
        }
        return menu;
    }

    void MainWindow::menuItemSelected (int menuItemID, int)
    {
        if (menuItemID == idExit)
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
        else if (menuItemID == idLoadDeck1 || menuItemID == idLoadDeck2)
        {
            if (mainComponent != nullptr)
                mainComponent->openLoadDialog (menuItemID == idLoadDeck1 ? 0 : 1);
        }
        else if (menuItemID == idAddToLibrary)
        {
            if (mainComponent != nullptr)
                mainComponent->showAddToLibraryMenu();
        }
        else if (menuItemID == idViewLargeLibrary)
        {
            if (mainComponent != nullptr)
                mainComponent->setLargeLibrary (! mainComponent->isLargeLibrary());
        }
        else if (menuItemID == idOptionsAudio)
        {
            showOptions (OptionsComponent::audioTab);
        }
        else if (menuItemID == idOptionsMixer)
        {
            showOptions (OptionsComponent::mixerTab);
        }
        else if (menuItemID == idOptionsGeneral)
        {
            showOptions (OptionsComponent::generalTab);
        }
        else if (menuItemID >= idLanguageBase && menuItemID < idAbout)
        {
            int i = 0;
            for (auto lang : I18n::allLanguages())
            {
                if (idLanguageBase + i++ == menuItemID)
                {
                    settings.setLanguage (lang);
                    I18n::getInstance().setLanguage (lang);
                    break;
                }
            }
        }
        else if (menuItemID == idAbout)
        {
            showAbout();
        }
    }

    // --- Diálogos -------------------------------------------------------------------
    void MainWindow::showOptions (int tab)
    {
        if (optionsWindow != nullptr)
        {
            if (auto* content = dynamic_cast<OptionsComponent*> (optionsWindow->getContentComponent()))
                content->showTab ((OptionsComponent::Tab) tab);
            optionsWindow->toFront (true);
            return;
        }

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new OptionsComponent (audio, settings, player.getEngine(), (OptionsComponent::Tab) tab));
        o.dialogTitle                  = OptionsComponent::getWindowTitle();
        o.dialogBackgroundColour       = ui::palette::panelDark;
        o.componentToCentreAround      = this;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar            = true;
        o.resizable                    = true;
        optionsWindow = o.launchAsync();

        if (optionsWindow != nullptr)
            optionsWindow->setResizeLimits (560, 460, 4096, 4096);
    }

    void MainWindow::showAbout()
    {
        const auto body = tr ("about.body", {
            { "version", juce::JUCEApplication::getInstance()->getApplicationVersion() },
            { "year",    juce::String (juce::Time::getCurrentTime().getYear()) } });

        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::InfoIcon)
                                          .withTitle (tr ("about.title"))
                                          .withMessage (body)
                                          .withButton (tr ("common.ok"))
                                          .withAssociatedComponent (this),
                                      nullptr);
    }

    void MainWindow::languageChanged()
    {
        setName (tr ("app.title"));
        menuItemsChanged();
    }
} // namespace kb::app
