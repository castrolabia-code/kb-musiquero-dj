#include "app/OptionsComponent.h"

#include "app/AppSettings.h"
#include "app/AudioDeviceService.h"
#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"

namespace kb::app
{
    // =========================================================================
    //  Pestaña Audio
    // =========================================================================
    class OptionsComponent::AudioPage final : public juce::Component,
                                              private juce::Timer
    {
    public:
        explicit AudioPage (AudioDeviceService& a) : audio (a)
        {
            hint.setJustificationType (juce::Justification::topLeft);
            hint.setColour (juce::Label::textColourId, ui::palette::textSecondary);
            latency.setColour (juce::Label::textColourId, ui::palette::cyan);
            latency.setFont (ui::KbLookAndFeel::labelFont (15.0f));
            addAndMakeVisible (hint);
            addAndMakeVisible (latency);
            rebuild();
            startTimerHz (4);
        }

        /** Recrea el selector (sus textos internos se traducen al construirse). */
        void rebuild()
        {
            // minIn, maxIn, minOut, maxOut, MIDI in, MIDI out, pares estéreo, ocultar avanzadas
            selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
                audio.getDeviceManager(), 0, 0, 2, AudioDeviceService::kMaxOutputChannels,
                false, false, true, false);
            addAndMakeVisible (*selector);
            hint.setText (tr ("options.audio.hint"), juce::dontSendNotification);
            timerCallback();
            resized();
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (12);
            latency.setBounds (b.removeFromBottom (26));
            hint.setBounds (b.removeFromBottom (44));
            if (selector != nullptr)
                selector->setBounds (b);
        }

    private:
        void timerCallback() override
        {
            const auto st = audio.getStatus();
            latency.setText (st.open ? tr ("options.audio.latency", { { "latency", juce::String (st.outputLatencyMs, 1) } })
                                     : tr ("options.audio.noDevice"),
                             juce::dontSendNotification);
        }

        AudioDeviceService& audio;
        std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
        juce::Label hint, latency;
    };

    // =========================================================================
    //  Pestaña Mixer (crossfader, ruteo de salidas, SPLIT)
    // =========================================================================
    class OptionsComponent::MixerPage final : public juce::Component,
                                              private juce::Timer
    {
    public:
        MixerPage (AudioDeviceService& a, AppSettings& s, engine::AudioEngine& e)
            : audio (a), settings (s), engine (e)
        {
            for (auto* l : { &curveLabel, &masterLabel, &phonesInfo, &warning })
                addAndMakeVisible (*l);
            warning.setColour (juce::Label::textColourId, ui::palette::alert);
            phonesInfo.setColour (juce::Label::textColourId, ui::palette::textSecondary);

            curveLabel.attachToComponent (&curveBox, true);
            masterLabel.attachToComponent (&masterBox, true);

            curveBox.onChange = [this]
            {
                const auto c = curveBox.getSelectedItemIndex();
                engine.setCrossfaderCurve ((engine::CrossfaderCurve) c);
                settings.setCrossfaderCurve (c);
            };
            reverseToggle.onClick = [this]
            {
                engine.setCrossfaderReverse (reverseToggle.getToggleState());
                settings.setCrossfaderReverse (reverseToggle.getToggleState());
            };
            masterBox.onChange = [this]
            {
                const auto p = masterBox.getSelectedItemIndex();
                engine.setMasterOutputPair (p);
                settings.setMasterOutputPair (p);
                timerCallback();
            };
            splitToggle.onClick = [this]
            {
                engine.setSplit (splitToggle.getToggleState());
                settings.setSplit (splitToggle.getToggleState());
                timerCallback();
            };

            for (auto* c : std::initializer_list<juce::Component*> { &curveBox, &reverseToggle, &masterBox, &splitToggle })
                addAndMakeVisible (c);

            refreshTexts();
            startTimerHz (4);
        }

        void refreshTexts()
        {
            curveLabel.setText (tr ("options.mixer.curve"), juce::dontSendNotification);
            masterLabel.setText (tr ("options.mixer.masterOut"), juce::dontSendNotification);
            reverseToggle.setButtonText (tr ("options.mixer.reverse"));
            splitToggle.setButtonText (tr ("options.mixer.split"));

            curveBox.clear (juce::dontSendNotification);
            curveBox.addItem (tr ("options.mixer.curve.smooth"), 1);
            curveBox.addItem (tr ("options.mixer.curve.cut"), 2);
            curveBox.addItem (tr ("options.mixer.curve.scratch"), 3);
            masterBox.clear (juce::dontSendNotification);
            masterBox.addItem (tr ("options.mixer.pair12"), 1);
            masterBox.addItem (tr ("options.mixer.pair34"), 2);

            curveBox.setSelectedItemIndex ((int) engine.getCrossfaderCurve(), juce::dontSendNotification);
            masterBox.setSelectedItemIndex (engine.getMasterOutputPair(), juce::dontSendNotification);
            reverseToggle.setToggleState (engine.getCrossfaderReverse(), juce::dontSendNotification);
            timerCallback();
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (16);
            b.removeFromLeft (220);   // etiquetas adjuntas
            curveBox.setBounds (b.removeFromTop (28).withWidth (260));
            b.removeFromTop (6);
            reverseToggle.setBounds (b.removeFromTop (26));
            b.removeFromTop (22);
            masterBox.setBounds (b.removeFromTop (28).withWidth (260));
            b.removeFromTop (4);
            phonesInfo.setBounds (b.removeFromTop (24));
            b.removeFromTop (10);
            splitToggle.setBounds (b.removeFromTop (26));
            b.removeFromTop (10);
            warning.setBounds (getLocalBounds().reduced (16).withTop (b.getY()).withHeight (60));
        }

    private:
        void timerCallback() override
        {
            const auto outputs = audio.getStatus().numOutputs;
            const auto split = engine.getSplit();
            splitToggle.setToggleState (split, juce::dontSendNotification);   // también se cambia desde el mixer

            const auto phonesPair = engine.getMasterOutputPair() == 0 ? "options.mixer.pair34" : "options.mixer.pair12";
            phonesInfo.setText (split ? tr ("options.mixer.phonesSplit")
                                      : tr ("options.mixer.phonesOut", { { "pair", tr (phonesPair) } }),
                                juce::dontSendNotification);

            juce::String warn;
            if (outputs > 0 && outputs < 4 && ! split)
                warn = tr ("options.mixer.twoOutputs");
            warning.setText (warn, juce::dontSendNotification);
        }

        AudioDeviceService& audio;
        AppSettings& settings;
        engine::AudioEngine& engine;
        juce::ComboBox curveBox, masterBox;
        juce::ToggleButton reverseToggle, splitToggle;
        juce::Label curveLabel, masterLabel, phonesInfo, warning;
    };

    // =========================================================================
    //  Pestaña General
    // =========================================================================
    class OptionsComponent::GeneralPage final : public juce::Component
    {
    public:
        explicit GeneralPage (AppSettings& s) : settings (s)
        {
            int id = 1;
            for (auto lang : I18n::allLanguages())
                languageBox.addItem (tr ("language." + I18n::toCode (lang)), id++);

            languageBox.setSelectedItemIndex (indexOf (I18n::getInstance().getLanguage()), juce::dontSendNotification);
            languageBox.onChange = [this]
            {
                const auto idx = languageBox.getSelectedItemIndex();
                int i = 0;
                for (auto lang : I18n::allLanguages())
                {
                    if (i++ == idx)
                    {
                        settings.setLanguage (lang);
                        // Diferido: el cambio de idioma refresca componentes, y no
                        // conviene hacerlo desde dentro del callback del ComboBox.
                        juce::MessageManager::callAsync ([lang] { I18n::getInstance().setLanguage (lang); });
                        break;
                    }
                }
            };

            languageLabel.attachToComponent (&languageBox, true);
            note.setColour (juce::Label::textColourId, ui::palette::textSecondary);
            addAndMakeVisible (languageBox);
            addAndMakeVisible (note);

            // Dibujo con la GPU (OpenGL): se aplica al instante.
            gpuToggle.setToggleState (settings.getUseOpenGL(), juce::dontSendNotification);
            gpuToggle.onClick = [this] { settings.setUseOpenGL (gpuToggle.getToggleState()); };
            gpuNote.setColour (juce::Label::textColourId, ui::palette::textSecondary);
            addAndMakeVisible (gpuToggle);
            addAndMakeVisible (gpuNote);
            refreshTexts();
        }

        void refreshTexts()
        {
            languageLabel.setText (tr ("options.language"), juce::dontSendNotification);
            note.setText (tr ("options.language.note"), juce::dontSendNotification);
            gpuToggle.setButtonText (tr ("options.gpu"));
            gpuNote.setText (tr ("options.gpu.note"), juce::dontSendNotification);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (16);
            b.removeFromLeft (200);   // espacio para la etiqueta adjunta
            languageBox.setBounds (b.removeFromTop (28).withWidth (240));
            b.removeFromTop (8);
            note.setBounds (b.removeFromTop (24));
            b.removeFromTop (24);
            gpuToggle.setBounds (b.removeFromTop (30).withWidth (560));
            gpuNote.setBounds (b.removeFromTop (44));
        }

    private:
        static int indexOf (Language l)
        {
            int i = 0;
            for (auto lang : I18n::allLanguages())
            {
                if (lang == l) return i;
                ++i;
            }
            return 0;
        }

        AppSettings& settings;
        juce::ComboBox languageBox;
        juce::Label languageLabel, note, gpuNote;
        juce::ToggleButton gpuToggle;
    };

    // =========================================================================
    //  OptionsComponent
    // =========================================================================
    OptionsComponent::OptionsComponent (AudioDeviceService& audio, AppSettings& settings,
                                        engine::AudioEngine& engine, Tab initialTab)
    {
        audioPage   = new AudioPage (audio);
        mixerPage   = new MixerPage (audio, settings, engine);
        generalPage = new GeneralPage (settings);

        const auto bg = ui::palette::panelDark;
        tabs.addTab (tr ("options.tab.audio"),   bg, audioPage,   true);
        tabs.addTab (tr ("options.tab.mixer"),   bg, mixerPage,   true);
        tabs.addTab (tr ("options.tab.general"), bg, generalPage, true);
        tabs.setTabBarDepth (32);
        addAndMakeVisible (tabs);

        showTab (initialTab);
        setSize (640, 560);
        I18n::getInstance().addListener (this);
    }

    OptionsComponent::~OptionsComponent()
    {
        I18n::getInstance().removeListener (this);
    }

    juce::String OptionsComponent::getWindowTitle()
    {
        return tr ("options.title") + " - " + tr ("app.title");
    }

    void OptionsComponent::showTab (Tab tab)
    {
        tabs.setCurrentTabIndex ((int) tab);
    }

    void OptionsComponent::resized()
    {
        tabs.setBounds (getLocalBounds());
    }

    void OptionsComponent::languageChanged()
    {
        tabs.setTabName (audioTab,   tr ("options.tab.audio"));
        tabs.setTabName (mixerTab,   tr ("options.tab.mixer"));
        tabs.setTabName (generalTab, tr ("options.tab.general"));
        audioPage->rebuild();
        mixerPage->refreshTexts();
        generalPage->refreshTexts();

        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->setName (getWindowTitle());
    }
} // namespace kb::app
