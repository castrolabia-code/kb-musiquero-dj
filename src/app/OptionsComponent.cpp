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
            refreshTexts();
        }

        void refreshTexts()
        {
            languageLabel.setText (tr ("options.language"), juce::dontSendNotification);
            note.setText (tr ("options.language.note"), juce::dontSendNotification);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (16);
            b.removeFromLeft (200);   // espacio para la etiqueta adjunta
            languageBox.setBounds (b.removeFromTop (28).withWidth (240));
            b.removeFromTop (8);
            note.setBounds (b.removeFromTop (24));
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
        juce::Label languageLabel, note;
    };

    // =========================================================================
    //  OptionsComponent
    // =========================================================================
    OptionsComponent::OptionsComponent (AudioDeviceService& audio, AppSettings& settings, Tab initialTab)
    {
        audioPage   = new AudioPage (audio);
        generalPage = new GeneralPage (settings);

        const auto bg = ui::palette::panelDark;
        tabs.addTab (tr ("options.tab.audio"),   bg, audioPage,   true);
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
        tabs.setTabName (generalTab, tr ("options.tab.general"));
        audioPage->rebuild();
        generalPage->refreshTexts();

        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
            window->setName (getWindowTitle());
    }
} // namespace kb::app
