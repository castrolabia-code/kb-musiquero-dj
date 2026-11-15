#include "app/MainComponent.h"

#include "app/AudioDeviceService.h"
#include "engine/AudioFileFormats.h"
#include "ui/Palette.h"

namespace kb::app
{
    static constexpr int kUiRefreshHz = 30;

    MainComponent::MainComponent (AudioDeviceService& a, PlayerController& p)
        : audio (a), player (p)
    {
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &topBar, &controller, &deckDisplays[0],
                                                                 &deckDisplays[1], &library, &statusBar })
            addAndMakeVisible (c);

        // --- Controlador -> motor --------------------------------------------------------
        auto& engine = player.getEngine();
        controller.onPlay       = [this] (int d) { player.togglePlay (d); };
        controller.onCueDown    = [this] (int d) { player.cueDown (d); };
        controller.onCueUp      = [this] (int d) { player.cueUp (d); };
        controller.onVolume     = [&engine] (int d, float v) { engine.setChannelVolume (d, v); };
        controller.onCrossfader = [&engine] (float v) { engine.setCrossfader (v); };
        controller.onMaster     = [&engine] (float v) { engine.setMasterVolume (v); };

        for (int d = 0; d < 2; ++d)
        {
            controller.setVolume (d, engine.getChannelVolume (d));
            deckDisplays[d].onSeek = [this, d] (double seconds) { player.seek (d, seconds); };
        }
        controller.setCrossfader (engine.getCrossfader());
        controller.setMaster (engine.getMasterVolume());

        I18n::getInstance().addListener (this);
        player.addListener (this);
        updateStatusBar();
        startTimerHz (kUiRefreshHz);
    }

    MainComponent::~MainComponent()
    {
        player.removeListener (this);
        I18n::getInstance().removeListener (this);
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        g.fillAll (ui::palette::panelDark);
    }

    void MainComponent::resized()
    {
        // Proporciones del layout de referencia. En la Fase 7 se suma el modo
        // "Biblioteca grande", que achica el controlador.
        auto b = getLocalBounds();

        statusBar.setBounds (b.removeFromBottom (juce::jlimit (18, 30, b.getHeight() / 40)));

        const auto avail = b.getHeight();
        topBar.setBounds     (b.removeFromTop (juce::roundToInt (avail * 0.06)));
        controller.setBounds (b.removeFromTop (juce::roundToInt (avail * 0.50)));

        auto waves = b.removeFromTop (juce::roundToInt (avail * 0.16)).reduced (2, 0);
        deckDisplays[0].setBounds (waves.removeFromTop (waves.getHeight() / 2));
        deckDisplays[1].setBounds (waves);

        library.setBounds (b);
    }

    // =========================================================================
    //  Carga de archivos
    // =========================================================================
    void MainComponent::openLoadDialog (int deck)
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("load.chooserTitle", { { "n", juce::String (deck + 1) } }),
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       engine::formats::fileWildcard());

        juce::Component::SafePointer<MainComponent> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe, deck] (const juce::FileChooser& fc)
                              {
                                  if (safe != nullptr && fc.getResult().existsAsFile())
                                      safe->player.loadFile (deck, fc.getResult());
                              });
    }

    void MainComponent::trackLoaded (int deck)
    {
        deckDisplays[deck].update (player.getTrack (deck), player.getStatus (deck), player.isLoading (deck));
    }

    void MainComponent::trackLoadFailed (int, const juce::File& file, engine::LoadError error)
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle (tr ("load.error.title"))
                                          .withMessage (tr ("load.error.body", { { "file", file.getFileName() },
                                                                                  { "reason", describeError (error) } }))
                                          .withButton (tr ("common.ok"))
                                          .withAssociatedComponent (this),
                                      nullptr);
    }

    juce::String MainComponent::describeError (engine::LoadError error)
    {
        using E = engine::LoadError;
        switch (error)
        {
            case E::fileNotFound:      return tr ("load.error.fileNotFound");
            case E::unsupportedFormat: return tr ("load.error.unsupportedFormat",
                                                  { { "formats", engine::formats::supportedExtensions().joinIntoString (", ").toUpperCase() } });
            case E::unreadable:        return tr ("load.error.unreadable");
            case E::empty:             return tr ("load.error.empty");
            case E::tooLong:           return tr ("load.error.tooLong");
            case E::outOfMemory:       return tr ("load.error.outOfMemory");
            case E::none:
            case E::cancelled:         break;
        }
        return {};
    }

    // =========================================================================
    //  Arrastrar y soltar desde el Explorador
    // =========================================================================
    bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
    {
        for (const auto& f : files)
            if (engine::formats::isSupportedFile (juce::File (f)))
                return true;
        return false;
    }

    int MainComponent::deckForDrop (int x, int y) const
    {
        const juce::Point<int> p (x, y);

        for (int d = 0; d < 2; ++d)
            if (deckDisplays[d].getBounds().contains (p))
                return d;

        if (controller.getBounds().contains (p))
        {
            const auto local = p - controller.getPosition();
            const auto d = controller.deckAt (local);
            if (d >= 0)
                return d;
            // Sobre el mixer: el lado más cercano.
            return local.x < controller.getWidth() / 2 ? 0 : 1;
        }

        // Biblioteca (Fase 6): todavía no recibe archivos.
        return -1;
    }

    void MainComponent::setDropTarget (int deck)
    {
        dropTarget = deck;
        controller.setDropTarget (deck);
        for (int d = 0; d < 2; ++d)
            deckDisplays[d].setDropHighlight (d == deck);
    }

    void MainComponent::fileDragEnter (const juce::StringArray&, int x, int y) { setDropTarget (deckForDrop (x, y)); }
    void MainComponent::fileDragMove (const juce::StringArray&, int x, int y)  { setDropTarget (deckForDrop (x, y)); }
    void MainComponent::fileDragExit (const juce::StringArray&)                { setDropTarget (-1); }

    void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
    {
        const auto deck = deckForDrop (x, y);
        setDropTarget (-1);
        if (deck < 0)
            return;

        // Se carga el primer archivo de audio válido.
        for (const auto& f : files)
        {
            const juce::File file (f);
            if (engine::formats::isSupportedFile (file))
            {
                player.loadFile (deck, file);
                return;
            }
        }
    }

    // =========================================================================
    //  Refresco de la UI
    // =========================================================================
    void MainComponent::timerCallback()
    {
        ++tick;
        const bool blink = (tick / (kUiRefreshHz / 4)) % 2 == 0;   // ~2 Hz

        for (int d = 0; d < 2; ++d)
        {
            const auto st = player.getStatus (d);
            const auto track = player.getTrack (d);

            ui::ControllerView::DeckLights lights;
            lights.loaded     = track != nullptr && st.trackId == track->getId();
            lights.playing    = st.playing;
            lights.cuePreview = st.cuePreview;
            lights.atCue      = std::abs (st.positionSeconds - st.cueSeconds) < 0.001;
            controller.setDeckLights (d, lights, blink);

            deckDisplays[d].update (track, st, player.isLoading (d));
        }

        if (tick % (kUiRefreshHz / 4) == 0)
            updateStatusBar();
    }

    void MainComponent::languageChanged()
    {
        updateStatusBar();
        // Los componentes traducen al pintar: alcanza con repintar.
        for (auto* c : getChildren())
            c->repaint();
        for (auto* c : controller.getChildren())
            c->repaint();
    }

    void MainComponent::updateStatusBar()
    {
        const auto st = audio.getStatus();
        juce::String left;

        if (st.open)
        {
            left = tr ("audio.status.running", {
                { "type",    st.typeName },
                { "device",  st.deviceName },
                { "rate",    juce::String (juce::roundToInt (st.sampleRate)) },
                { "buffer",  juce::String (st.bufferSize) },
                { "latency", juce::String (st.outputLatencyMs, 1) },
                { "outputs", juce::String (st.numOutputs) },
                { "cpu",     juce::String (st.cpuPercent, 1) } });
        }
        else
        {
            left = tr ("audio.status.noDevice");
        }

        statusBar.setText (left, "v" + juce::JUCEApplication::getInstance()->getApplicationVersion());
    }
} // namespace kb::app
