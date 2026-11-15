#include "app/MainComponent.h"

#include "app/AudioDeviceService.h"
#include "engine/AudioFileFormats.h"
#include "analysis/KeyDetector.h"
#include "ui/Palette.h"
#include "ui/TribalArt.h"

namespace kb::app
{
    static constexpr int kUiRefreshHz = 30;

    MainComponent::MainComponent (AudioDeviceService& a, PlayerController& p, TrackLibrary& lib, AppSettings& s)
        : audio (a), settings (s), player (p), library (lib), libraryView (lib)
    {
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &topBar, &controller, &deckDisplays[0],
                                                                 &deckDisplays[1], &libraryView, &statusBar })
            addAndMakeVisible (c);

        libraryView.onLoadRequest = [this] (int deck, const juce::File& file)
        {
            player.loadFile (deck >= 0 ? deck : chooseFreeDeck(), file);
        };

        // --- Controlador -> motor --------------------------------------------------------
        auto& engine = player.getEngine();
        controller.onPlay       = [this] (int d) { player.play (d); };
        controller.onPause      = [this] (int d) { player.pause (d); };
        controller.onCueDown    = [this] (int d) { player.cueDown (d); };
        controller.onCueUp      = [this] (int d) { player.cueUp (d); };
        controller.onVolume     = [&engine] (int d, float v) { engine.setChannelVolume (d, v); };
        controller.onCrossfader = [&engine] (float v) { engine.setCrossfader (v); };
        controller.onMaster     = [&engine] (float v) { engine.setMasterVolume (v); };
        controller.onTrim       = [&engine] (int d, float v) { engine.setTrim (d, v); };
        controller.onEq         = [&engine] (int d, int band, float v) { engine.setEq (d, (engine::EqBand) band, v); };
        controller.onFilter     = [&engine] (int d, float v) { engine.setFilter (d, v); };
        controller.onChannelCue = [&engine] (int d, bool on) { engine.setChannelCue (d, on); };
        controller.onPhones     = [&engine] (float v) { engine.setHeadphoneVolume (v); };
        controller.onCueMix     = [&engine] (float v) { engine.setCueMix (v); };
        controller.onSplit      = [&engine] (bool on) { engine.setSplit (on); };
        controller.onTempo      = [this] (int d, float pct) { player.setTempo (d, pct); };
        controller.onSync       = [this] (int d) { player.toggleSync (d); };
        controller.onKeyLock    = [&engine] (int d) { engine.setKeyLock (d, ! engine.getKeyLock (d)); };
        controller.onKeyShift   = [&engine] (int d, int delta)
        {
            engine.setKeyShift (d, delta == 0 ? 0 : engine.getKeyShift (d) + delta);
        };

        // --- AUTO MIX ------------------------------------------------------------------------
        autoMixSeconds = settings.getAutoMixSeconds();
        controller.setAutoMixSeconds (autoMixSeconds);
        controller.onAutoMix     = [this] { toggleAutoMix(); };
        controller.onAutoMixTime = [this] (double secs)
        {
            autoMixSeconds = secs;
            settings.setAutoMixSeconds (secs);
        };

        for (int d = 0; d < 2; ++d)
        {
            controller.setVolume (d, engine.getChannelVolume (d));
            controller.setTrim (d, engine.getTrim (d));
            controller.setFilter (d, engine.getFilter (d));
            for (int band = 0; band < 3; ++band)
                controller.setEq (d, band, engine.getEq (d, (engine::EqBand) band));
            controller.setChannelCue (d, engine.getChannelCue (d));
            deckDisplays[d].onSeek = [this, d] (double seconds) { player.seek (d, seconds); };
        }
        controller.setCrossfader (engine.getCrossfader());
        controller.setMaster (engine.getMasterVolume());
        controller.setPhones (engine.getHeadphoneVolume());
        controller.setCueMix (engine.getCueMix());
        controller.setSplit (engine.getSplit());

        // Vúmetros a 60 fps sincronizados con el refresco de la pantalla.
        vblank = std::make_unique<juce::VBlankAttachment> (this, [this] { updateMeters(); });

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

    int MainComponent::frameThickness() const
    {
        return juce::jmax (14, juce::roundToInt (getHeight() * 0.028));
    }

    void MainComponent::paint (juce::Graphics& g)
    {
        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (frameImage.isNull() || ! juce::exactlyEqual (scale, frameScale)
            || frameImage.getWidth() != juce::roundToInt ((float) getWidth() * scale))
            renderFrame (scale);
        g.drawImage (frameImage, getLocalBounds().toFloat());
    }

    void MainComponent::renderFrame (float scale)
    {
        using namespace ui::palette;
        frameScale = scale;
        frameImage = juce::Image (juce::Image::RGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                                  juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
        juce::Graphics g (frameImage);
        g.addTransform (juce::AffineTransform::scale (scale));

        const auto b = getLocalBounds().toFloat();
        const auto f = (float) frameThickness();
        const auto outerR = f * 2.2f;
        const auto inner = b.reduced (f);

        g.fillAll (panelDark);

        // Marco amarillo con volumen (degradé + bisel).
        juce::Path frame;
        frame.addRoundedRectangle (b.reduced (1.0f), outerR);
        juce::Path hole;
        hole.addRoundedRectangle (inner.expanded (1.0f), outerR * 0.55f);
        frame.addPath (hole);
        frame.setUsingNonZeroWinding (false);

        juce::ColourGradient grad (chassisLight, 0.0f, 0.0f, chassisShadow, 0.0f, b.getBottom(), false);
        grad.addColour (0.4, chassis);
        g.setGradientFill (grad);
        g.fillPath (frame);

        // Tribales negros sobre el marco: bandas en los cuatro lados y esquineros.
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (frame);
            g.setColour (juce::Colours::black.withAlpha (0.86f));
            const auto bandH = f * 0.9f;
            const auto margin = outerR * 1.6f;
            g.fillPath (ui::tribal::band ({ margin, (f - bandH) * 0.5f, b.getWidth() - 2.0f * margin, bandH }));
            g.fillPath (ui::tribal::band ({ margin, b.getBottom() - f + (f - bandH) * 0.5f, b.getWidth() - 2.0f * margin, bandH }));

            const auto side = ui::tribal::band ({ 0.0f, 0.0f, b.getHeight() - 2.0f * margin, bandH });
            g.fillPath (side, juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                  .translated ((f + bandH) * 0.5f, margin));
            g.fillPath (side, juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                  .translated (b.getRight() - (f - bandH) * 0.5f, margin));

            const auto cs = outerR * 1.9f;
            g.fillPath (ui::tribal::corner ({ 0.0f, 0.0f, cs, cs }, 0));
            g.fillPath (ui::tribal::corner ({ b.getRight() - cs, 0.0f, cs, cs }, 1));
            g.fillPath (ui::tribal::corner ({ b.getRight() - cs, b.getBottom() - cs, cs, cs }, 2));
            g.fillPath (ui::tribal::corner ({ 0.0f, b.getBottom() - cs, cs, cs }, 3));
        }

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawRoundedRectangle (b.reduced (2.0f), outerR, 1.5f);
        g.setColour (chassisShadow.darker (0.6f));
        g.drawRoundedRectangle (inner.expanded (1.0f), outerR * 0.55f, 2.0f);
    }

    void MainComponent::resized()
    {
        // Proporciones del layout de referencia. En la Fase 7 se suma el modo
        // "Biblioteca grande", que achica el controlador.
        frameImage = {};
        auto b = getLocalBounds().reduced (frameThickness());

        statusBar.setBounds (b.removeFromBottom (juce::jlimit (18, 30, b.getHeight() / 40)));

        const auto avail = b.getHeight();
        topBar.setBounds     (b.removeFromTop (juce::roundToInt (avail * 0.08)));
        controller.setBounds (b.removeFromTop (juce::roundToInt (avail * 0.46)));

        auto waves = b.removeFromTop (juce::roundToInt (avail * 0.20)).reduced (2, 0);
        deckDisplays[0].setBounds (waves.removeFromTop (waves.getHeight() / 2));
        deckDisplays[1].setBounds (waves);

        libraryView.setBounds (b);
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
        deckDisplays[deck].update (makeDeckView (deck));
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

    void MainComponent::toggleAutoMix()
    {
        auto& engine = player.getEngine();
        if (engine.isAutoMixing())
        {
            engine.cancelAutoMix();
            return;
        }

        const auto source = engine::AudioEngine::autoMixSource (engine.getCrossfader(), engine.getCrossfaderReverse(),
                                                                engine.getDeckStatus (0).playing,
                                                                engine.getDeckStatus (1).playing);
        const auto target = 1 - source;
        if (player.getTrack (target) == nullptr)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::InfoIcon)
                                              .withTitle (tr ("automix.title"))
                                              .withMessage (tr ("automix.needTrack", { { "deck", juce::String (target + 1) } }))
                                              .withButton (tr ("common.ok"))
                                              .withAssociatedComponent (this),
                                          nullptr);
            return;
        }
        engine.startAutoMix (autoMixSeconds);
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
        {
            const juce::File file (f);
            if (engine::formats::isSupportedFile (file) || file.isDirectory())
                return true;
        }
        return false;
    }

    int MainComponent::chooseFreeDeck() const
    {
        const auto a = player.getStatus (0), b = player.getStatus (1);
        if (a.playing && ! b.playing) return 1;
        if (! a.playing && b.playing) return 0;
        if (player.getTrack (0) != nullptr && player.getTrack (1) == nullptr) return 1;
        return 0;
    }

    // --- Filas de la biblioteca arrastradas a un deck ------------------------------------
    bool MainComponent::isInterestedInDragSource (const SourceDetails& d)
    {
        return d.description.toString().startsWith (LibraryView::kDragPrefix);
    }

    void MainComponent::itemDragEnter (const SourceDetails& d) { itemDragMove (d); }
    void MainComponent::itemDragMove (const SourceDetails& d)
    {
        const auto deck = deckForDrop (d.localPosition.x, d.localPosition.y);
        setDropTarget (deck >= 0 ? deck : -1);
    }
    void MainComponent::itemDragExit (const SourceDetails&) { setDropTarget (-1); }

    void MainComponent::itemDropped (const SourceDetails& d)
    {
        const auto deck = deckForDrop (d.localPosition.x, d.localPosition.y);
        setDropTarget (-1);
        if (deck >= 0)
            player.loadFile (deck, juce::File (d.description.toString().fromFirstOccurrenceOf (LibraryView::kDragPrefix, false, false)));
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

        if (libraryView.getBounds().contains (p))
            return kLibraryTarget;
        return -1;
    }

    void MainComponent::setDropTarget (int deck)
    {
        dropTarget = deck;
        controller.setDropTarget (deck);
        for (int d = 0; d < 2; ++d)
            deckDisplays[d].setDropHighlight (d == deck);
    }

    void MainComponent::fileDragEnter (const juce::StringArray&, int x, int y) { setDropTarget (juce::jmax (-1, deckForDrop (x, y))); }
    void MainComponent::fileDragMove (const juce::StringArray&, int x, int y)  { setDropTarget (juce::jmax (-1, deckForDrop (x, y))); }
    void MainComponent::fileDragExit (const juce::StringArray&)                { setDropTarget (-1); }

    void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
    {
        const auto deck = deckForDrop (x, y);
        setDropTarget (-1);

        if (deck == kLibraryTarget)
        {
            // Sobre la biblioteca: se agregan los archivos (y las carpetas, con todo su contenido).
            juce::Array<juce::File> toAdd;
            for (const auto& f : files)
            {
                const juce::File file (f);
                if (file.isDirectory())
                    library.addFolder (file);
                else
                    toAdd.add (file);
            }
            library.addFiles (toAdd);
            return;
        }
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

            auto& engine = player.getEngine();
            controller.setTempo (d, engine.getTempo (d));
            controller.setSyncLit (d, engine.getSync (d));
            controller.setKeyState (d, engine.getKeyLock (d), engine.getKeyShift (d));
        }

        // Colores de compatibilidad armónica en la lista: respecto de lo que está sonando.
        std::vector<int> playingKeys;
        for (int d = 0; d < 2; ++d)
        {
            const auto v = makeDeckView (d);
            if (v.track != nullptr && v.status.playing && v.track->getKey() >= 0)
                playingKeys.push_back (analysis::MusicalKey::transpose (
                    v.track->getKey(), ui::DeckDisplay::effectiveShift (v.tempo, v.keyLock, v.keyShift)));
        }
        libraryView.setReferenceKeys (playingKeys);

        // El AUTO MIX mueve el crossfader solo: la perilla lo sigue.
        controller.setCrossfader (player.getEngine().getCrossfader());
        controller.setAutoMixState (player.getEngine().getAutoMixProgress());

        // SPLIT también se cambia desde Opciones: reflejarlo en el botón.
        controller.setSplit (player.getEngine().getSplit());

        if (tick % (kUiRefreshHz / 4) == 0)
            updateStatusBar();
    }

    ui::DeckDisplay::View MainComponent::makeDeckView (int d) const
    {
        auto& engine = player.getEngine();
        ui::DeckDisplay::View v;
        v.track     = player.getTrack (d);
        v.status    = engine.getDeckStatus (d);
        v.loading   = player.isLoading (d);
        v.analysing = player.isAnalysing (d);
        v.tempo     = engine.getTempo (d);
        v.keyLock   = engine.getKeyLock (d);
        v.keyShift  = engine.getKeyShift (d);
        return v;
    }

    void MainComponent::updateMeters()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const auto dt  = lastMeterTime > 0.0 ? (float) juce::jlimit (0.0, 0.1, now - lastMeterTime) : 1.0f / 60.0f;
        lastMeterTime = now;

        auto& engine = player.getEngine();
        for (int d = 0; d < 2; ++d)
        {
            const auto st = engine.getDeckStatus (d);
            controller.setPlatter (d, st.positionSeconds, st.playing, st.trackId != 0);
            deckDisplays[d].update (makeDeckView (d));   // waveform ampliada a 60 fps
        }
        controller.pushChannelMeter (0, engine.readMeter (engine::MeterId::channel1), dt);
        controller.pushChannelMeter (1, engine.readMeter (engine::MeterId::channel2), dt);
        topBar.pushMaster (engine.readMeter (engine::MeterId::masterLeft),
                           engine.readMeter (engine::MeterId::masterRight),
                           engine.getMasterLimiterReductionDb(), dt);
    }

    void MainComponent::languageChanged()
    {
        updateStatusBar();
        controller.invalidateBackground();
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
