#include "app/MainComponent.h"

#include "app/AudioDeviceService.h"
#include "ui/Palette.h"

namespace kb::app
{
    MainComponent::MainComponent (AudioDeviceService& a)
        : audio (a)
    {
        setOpaque (true);
        for (auto* c : std::initializer_list<juce::Component*> { &topBar, &controller, &waveforms, &library, &statusBar })
            addAndMakeVisible (c);

        I18n::getInstance().addListener (this);
        updateStatus();
        startTimerHz (4);
    }

    MainComponent::~MainComponent()
    {
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
        waveforms.setBounds  (b.removeFromTop (juce::roundToInt (avail * 0.16)));
        library.setBounds    (b);
    }

    void MainComponent::languageChanged()
    {
        updateStatus();
        // Los placeholders traducen al pintar: alcanza con repintar.
        for (auto* c : getChildren())
            c->repaint();
    }

    void MainComponent::timerCallback()
    {
        updateStatus();
    }

    void MainComponent::updateStatus()
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
