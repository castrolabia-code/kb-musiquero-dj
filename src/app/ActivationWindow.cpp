#include "app/ActivationWindow.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"

namespace kb::app
{
    using namespace ui::palette;

    namespace
    {
        class Content final : public juce::Component
        {
        public:
            Content (const juce::String& previousKey, licensing::Status previousStatus,
                     std::function<void (const juce::String&, const licensing::Result&)> onOk, std::function<void()> onQuit)
                : ok (std::move (onOk)), quit (std::move (onQuit))
            {
                keyBox.setMultiLine (true, true);
                keyBox.setReturnKeyStartsNewLine (false);
                keyBox.setText (previousKey, false);
                keyBox.setTextToShowWhenEmpty (tr ("license.placeholder"), textSecondary);
                keyBox.setFont (ui::KbLookAndFeel::labelFont (16.0f, false));
                keyBox.onReturnKey = [this] { activate(); };
                addAndMakeVisible (keyBox);

                machine.setText (tr ("license.machine", { { "id", licensing::machineId() } }), juce::dontSendNotification);
                machine.setColour (juce::Label::textColourId, textSecondary);
                machine.setEditable (false, false);
                addAndMakeVisible (machine);
                copyButton.setButtonText (tr ("license.copyMachine"));
                copyButton.onClick = [] { juce::SystemClipboard::copyTextToClipboard (licensing::machineId()); };
                addAndMakeVisible (copyButton);

                if (previousStatus != licensing::Status::missing && previousStatus != licensing::Status::valid)
                    status.setText (tr (licensing::statusKey (previousStatus)), juce::dontSendNotification);
                status.setColour (juce::Label::textColourId, alert);
                addAndMakeVisible (status);

                activateButton.setButtonText (tr ("license.activate"));
                activateButton.onClick = [this] { activate(); };
                quitButton.setButtonText (tr ("license.quit"));
                quitButton.onClick = [this] { if (quit) quit(); };
                addAndMakeVisible (activateButton);
                addAndMakeVisible (quitButton);
                setSize (680, 400);
            }

            void paint (juce::Graphics& g) override
            {
                g.fillAll (panelDark);
                auto b = getLocalBounds().toFloat().reduced (24.0f);
                auto top = b.removeFromTop (70.0f);
                g.setColour (chassis);
                g.fillRoundedRectangle (top, 10.0f);
                ui::draw::text (g, tr ("app.logo"), top.reduced (16.0f, 6.0f), 30.0f, juce::Colours::black,
                                juce::Justification::centredLeft);
                ui::draw::text (g, tr ("app.brand"), top.reduced (16.0f, 6.0f), 14.0f, juce::Colours::black,
                                juce::Justification::centredRight);
                ui::draw::text (g, tr ("license.title"), b.removeFromTop (44.0f), 20.0f, textPrimary, juce::Justification::centredLeft);
            }

            void resized() override
            {
                auto b = getLocalBounds().reduced (24);
                b.removeFromTop (70 + 44);
                keyBox.setBounds (b.removeFromTop (110));
                b.removeFromTop (8);
                auto m = b.removeFromTop (28);
                copyButton.setBounds (m.removeFromRight (150));
                machine.setBounds (m);
                b.removeFromTop (6);
                status.setBounds (b.removeFromTop (28));
                auto buttons = b.removeFromBottom (40);
                quitButton.setBounds (buttons.removeFromLeft (140));
                activateButton.setBounds (buttons.removeFromRight (180));
            }

        private:
            void activate()
            {
                const auto key = keyBox.getText();
                const auto r = licensing::verifyLicense (key, licensing::productPublicKey(), licensing::machineId());
                if (! r.isValid())
                {
                    status.setText (tr (licensing::statusKey (r.status)), juce::dontSendNotification);
                    return;
                }
                if (ok) ok (key.removeCharacters (" \t\r\n"), r);
            }

            std::function<void (const juce::String&, const licensing::Result&)> ok;
            std::function<void()> quit;
            juce::TextEditor keyBox;
            juce::Label machine, status;
            juce::TextButton copyButton, activateButton, quitButton;
        };
    }

    ActivationWindow::ActivationWindow (const juce::String& previousKey, licensing::Status previousStatus,
                                        std::function<void (const juce::String&, const licensing::Result&)> onActivated,
                                        std::function<void()> onQuit)
        : juce::DocumentWindow (tr ("license.window"), ui::palette::panelDark, juce::DocumentWindow::closeButton),
          quit (onQuit)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new Content (previousKey, previousStatus, std::move (onActivated), std::move (onQuit)), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void ActivationWindow::closeButtonPressed()
    {
        if (quit) quit();
    }
} // namespace kb::app
