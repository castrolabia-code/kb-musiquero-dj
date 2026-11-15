// =============================================================================
//  KB - MUSIQUERO DJ · by Castro Labs
//  Punto de entrada de la aplicación de escritorio.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/AppSettings.h"
#include "app/AudioDeviceService.h"
#include "app/I18n.h"
#include "app/MainWindow.h"
#include "engine/AudioEngine.h"
#include "ui/KbLookAndFeel.h"

namespace kb::app
{
    class KbMusiqueroApplication final : public juce::JUCEApplication
    {
    public:
        KbMusiqueroApplication() = default;

        const juce::String getApplicationName() override    { return KB_PRODUCT_NAME; }
        const juce::String getApplicationVersion() override { return KB_VERSION_STRING; }
        bool moreThanOneInstanceAllowed() override          { return false; }

        void initialise (const juce::String&) override
        {
            settings = std::make_unique<AppSettings>();
            initialiseLogger();

            // Idioma: el guardado, o el del sistema la primera vez.
            const auto language = settings->getLanguage().value_or (I18n::detectSystemLanguage());
            settings->setLanguage (language);
            I18n::getInstance().setLanguage (language);

            lookAndFeel = std::make_unique<ui::KbLookAndFeel>();
            juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

            engine = std::make_unique<engine::AudioEngine>();
            audio  = std::make_unique<AudioDeviceService> (*engine, *settings);
            const auto audioError = audio->initialise();

            mainWindow = std::make_unique<MainWindow> (*settings, *audio);

            // Si el audio no abrió, avisar y llevar directo a Opciones > Audio.
            if (audioError.isNotEmpty())
            {
                juce::Component::SafePointer<MainWindow> window (mainWindow.get());
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                  .withTitle (tr ("app.title"))
                                                  .withMessage (tr ("audio.error.open", { { "error", audioError } }))
                                                  .withButton (tr ("common.ok"))
                                                  .withAssociatedComponent (mainWindow.get()),
                                              [window] (int)
                                              {
                                                  if (window != nullptr)
                                                      window->showOptions (0);
                                              });
            }
        }

        void shutdown() override
        {
            mainWindow.reset();
            audio.reset();      // guarda el estado del dispositivo y cierra el audio
            engine.reset();

            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
            lookAndFeel.reset();

            if (settings != nullptr)
                settings->flush();
            settings.reset();

            I18n::shutdown();

            juce::Logger::writeToLog ("Cierre normal.");
            juce::Logger::setCurrentLogger (nullptr);
            logger.reset();
        }

        void systemRequestedQuit() override
        {
            quit();
        }

        void anotherInstanceStarted (const juce::String&) override
        {
            // Solo una instancia: traer la ventana existente al frente.
            if (mainWindow != nullptr)
            {
                mainWindow->setMinimised (false);
                mainWindow->toFront (true);
            }
        }

    private:
        void initialiseLogger()
        {
            const auto logFile = AppSettings::getAppDataDirectory().getChildFile ("logs").getChildFile ("kb-musiquero-dj.log");
            logFile.getParentDirectory().createDirectory();

            logger = std::make_unique<juce::FileLogger> (
                logFile,
                getApplicationName() + " " + getApplicationVersion() + " - "
                    + juce::SystemStats::getOperatingSystemName() + " - "
                    + juce::Time::getCurrentTime().toString (true, true),
                1024 * 1024);   // máximo 1 MB: se recorta al arrancar
            juce::Logger::setCurrentLogger (logger.get());
        }

        std::unique_ptr<AppSettings>          settings;
        std::unique_ptr<juce::FileLogger>     logger;
        std::unique_ptr<ui::KbLookAndFeel>    lookAndFeel;
        std::unique_ptr<engine::AudioEngine>  engine;
        std::unique_ptr<AudioDeviceService>   audio;
        std::unique_ptr<MainWindow>           mainWindow;
    };
} // namespace kb::app

START_JUCE_APPLICATION (kb::app::KbMusiqueroApplication)
