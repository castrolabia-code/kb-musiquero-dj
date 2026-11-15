// =============================================================================
//  KB - MUSIQUERO DJ · by Castro Labs
//  Punto de entrada de la aplicación de escritorio.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/AnalysisCache.h"
#include "app/AppSettings.h"
#include "app/AudioDeviceService.h"
#include "app/I18n.h"
#include "app/MainWindow.h"
#include "app/PlayerController.h"
#include "app/TrackLibrary.h"
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
            engine->setCrossfaderCurve ((engine::CrossfaderCurve) settings->getCrossfaderCurve());
            engine->setCrossfaderReverse (settings->getCrossfaderReverse());
            engine->setMasterOutputPair (settings->getMasterOutputPair());
            engine->setSplit (settings->getSplit());
            audio  = std::make_unique<AudioDeviceService> (*engine, *settings);
            const auto audioError = audio->initialise();

            const auto dataDir = AppSettings::getAppDataDirectory();
            analysisCache = std::make_unique<AnalysisCache> (dataDir.getChildFile ("analysis.json"));
            library = std::make_unique<TrackLibrary> (*analysisCache, dataDir.getChildFile ("library.json"));
            player = std::make_unique<PlayerController> (*engine, *analysisCache);
            mainWindow = std::make_unique<MainWindow> (*settings, *audio, *player, *library);

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
            player.reset();     // cancela cargas y análisis en curso
            library.reset();    // guarda la lista
            analysisCache.reset();   // guarda los BPM analizados

            if (engine != nullptr && settings != nullptr)
            {
                // SPLIT también se cambia desde el mixer: se guarda el estado final.
                settings->setSplit (engine->getSplit());
            }
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
        std::unique_ptr<AnalysisCache>        analysisCache;
        std::unique_ptr<TrackLibrary>         library;
        std::unique_ptr<PlayerController>     player;
        std::unique_ptr<MainWindow>           mainWindow;
    };
} // namespace kb::app

START_JUCE_APPLICATION (kb::app::KbMusiqueroApplication)
