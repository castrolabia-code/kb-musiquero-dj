#include "ui/FxPanel.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

namespace kb::ui
{
    using namespace palette;

    namespace
    {
        const char* const kEffectKeys[] = { "fx.echo", "fx.delay", "fx.reverb", "fx.flanger", "fx.phaser", "fx.filter",
                                            "fx.bitcrusher", "fx.gater", "fx.roll", "fx.brake", "fx.backspin", "fx.noise" };
        const char* const kParamKeys[]  = { "fx.p.feedback", "fx.p.feedback", "fx.p.room", "fx.p.feedback", "fx.p.feedback", "fx.p.depth",
                                            "fx.p.crush", "fx.p.depth", "fx.p.none", "fx.p.none", "fx.p.none", "fx.p.freq" };
        constexpr int kNumEffects = 12;
        constexpr int kNumBeats = 7;
    }

    const char* FxPanel::effectKey (int t) { return kEffectKeys[juce::jlimit (0, kNumEffects - 1, t)]; }
    const char* FxPanel::paramKey (int t)  { return kParamKeys[juce::jlimit (0, kNumEffects - 1, t)]; }

    juce::String FxPanel::beatsText (int i)
    {
        static const char* labels[] = { "1/8", "1/4", "1/2", "1", "2", "4", "8" };
        return labels[juce::jlimit (0, kNumBeats - 1, i)];
    }

    FxPanel::FxPanel()
    {
        setOpaque (false);
        for (int u = 0; u < kNumUnits; ++u)
        {
            auto& U = units[(size_t) u];

            U.type.setTooltip (tr ("tip.fxType"));
            U.type.onChange = [this, u]
            {
                const auto id = units[(size_t) u].type.getSelectedId();
                if (id > 0 && onType) onType (u, id - 1);
            };
            addAndMakeVisible (U.type);

            U.beatsDown = std::make_unique<GlowButton> (juce::String(), cyan, false);
            U.beatsUp   = std::make_unique<GlowButton> (juce::String(), cyan, false);
            U.beatsDown->setLabelText ("<");
            U.beatsUp->setLabelText (">");
            for (auto* b : { U.beatsDown.get(), U.beatsUp.get() })
            {
                b->setTooltipKey ("tip.fxBeats");
                b->setTextScale (0.6f);
                addAndMakeVisible (*b);
            }
            U.beatsDown->onPress = [this, u] { if (onBeats) onBeats (u, juce::jmax (0, units[(size_t) u].state.beatsIndex - 1)); };
            U.beatsUp->onPress   = [this, u] { if (onBeats) onBeats (u, juce::jmin (kNumBeats - 1, units[(size_t) u].state.beatsIndex + 1)); };

            U.on = std::make_unique<GlowButton> ("fx.on", play, false);
            U.on->setTooltipKey ("tip.fxOn");
            U.on->onPress = [this, u] { if (onOn) onOn (u, ! units[(size_t) u].state.on); };
            addAndMakeVisible (*U.on);

            static const char* targetKeys[] = { "fx.t.deck1", "fx.t.deck2", "fx.t.master" };
            for (int t = 0; t < 3; ++t)
            {
                auto b = std::make_unique<GlowButton> (targetKeys[t], orange, false);
                b->setTooltipKey ("tip.fxTarget");
                b->onPress = [this, u, t] { if (onTarget) onTarget (u, t); };
                addAndMakeVisible (*b);
                U.targets[(size_t) t] = std::move (b);
            }

            U.param = std::make_unique<KbKnob> ("fx.p.feedback");
            U.wet   = std::make_unique<KbKnob> ("fx.wet");
            U.param->setTooltipKey ("tip.fxParam");
            U.wet->setTooltipKey ("tip.fxWet");
            U.param->setLabelSide (+1);
            U.wet->setLabelSide (+1);
            U.param->setDoubleClickReturnValue (true, 0.5);
            U.wet->setDoubleClickReturnValue (true, 0.5);
            const auto fxId = "fx" + juce::String (u + 1) + ".";
            U.on->setComponentID (fxId + "on");
            U.param->setComponentID (fxId + "param");
            U.wet->setComponentID (fxId + "wet");
            U.beatsDown->setComponentID (fxId + "beatsDown");
            U.beatsUp->setComponentID (fxId + "beatsUp");
            U.param->onValueChange = [this, u] { if (onParam) onParam (u, (float) units[(size_t) u].param->getValue()); };
            U.wet->onValueChange   = [this, u] { if (onWet) onWet (u, (float) units[(size_t) u].wet->getValue()); };
            addAndMakeVisible (*U.param);
            addAndMakeVisible (*U.wet);
        }
        refreshTexts();
    }

    void FxPanel::refreshTexts()
    {
        for (int u = 0; u < kNumUnits; ++u)
        {
            auto& combo = units[(size_t) u].type;
            const auto selected = combo.getSelectedId();
            combo.clear (juce::dontSendNotification);
            for (int t = 0; t < kNumEffects; ++t)
                combo.addItem (tr (kEffectKeys[t]), t + 1);
            combo.setSelectedId (selected > 0 ? selected : units[(size_t) u].state.type + 1, juce::dontSendNotification);
            combo.setTooltip (tr ("tip.fxType"));
        }
        repaint();
    }

    void FxPanel::setUnit (int u, const UnitState& s)
    {
        if (! juce::isPositiveAndBelow (u, kNumUnits))
            return;
        auto& U = units[(size_t) u];
        const auto beatsChanged = s.beatsIndex != U.state.beatsIndex || s.usesBeats != U.state.usesBeats;
        const auto typeChanged = s.type != U.state.type || ! formatterSet[(size_t) u];
        U.state = s;
        if (U.type.getSelectedId() != s.type + 1)
            U.type.setSelectedId (s.type + 1, juce::dontSendNotification);
        U.on->setLit (s.on);
        for (int t = 0; t < 3; ++t)
            U.targets[(size_t) t]->setLit (t == s.target);
        if (! U.param->isMouseButtonDown())
            U.param->setValue (s.param, juce::dontSendNotification);
        if (! U.wet->isMouseButtonDown())
            U.wet->setValue (s.wet, juce::dontSendNotification);
        if (typeChanged)
        {
            const auto pk = juce::String (paramKey (s.type));
            U.param->setLabelFormatter ([pk] (double) { return tr (pk); });
            U.param->setEnabled (pk != "fx.p.none");
            formatterSet[(size_t) u] = true;
        }
        U.beatsDown->setEnabled (s.usesBeats);
        U.beatsUp->setEnabled (s.usesBeats);
        if (beatsChanged)
            repaint (U.beatsArea.toNearestInt().expanded (2));
    }

    void FxPanel::resized()
    {
        auto b = getLocalBounds().toFloat();
        const auto h = b.getHeight();
        const auto gap = h * 0.6f;
        const auto unitW = (b.getWidth() - gap) / 2.0f;
        for (int u = 0; u < kNumUnits; ++u)
        {
            auto& U = units[(size_t) u];
            auto r = u == 0 ? b.withWidth (unitW) : b.withTrimmedLeft (unitW + gap);
            r = r.reduced (h * 0.12f, h * 0.1f);
            const auto rh = r.getHeight();

            // Pantallas angostas (teléfono, Fase 10): todo se angosta en proporción.
            const auto k = juce::jlimit (0.55f, 1.0f, r.getWidth() / (rh * 21.0f));
            U.title = r.removeFromLeft (rh * 1.6f * k);
            r.removeFromLeft (rh * 0.15f * k);
            U.type.setBounds (r.removeFromLeft (rh * 4.6f * k).reduced (0.0f, rh * 0.08f).toNearestInt());
            r.removeFromLeft (rh * 0.3f * k);

            auto beats = r.removeFromLeft (rh * 4.0f * k);
            U.beatsDown->setBounds (beats.removeFromLeft (rh * 0.9f).toNearestInt());
            U.beatsUp->setBounds (beats.removeFromRight (rh * 0.9f).toNearestInt());
            U.beatsArea = beats;
            r.removeFromLeft (rh * 0.3f * k);

            // Destinos y ON a la derecha.
            auto right = r.removeFromRight (rh * 6.6f * k);
            for (int t = 2; t >= 0; --t)
            {
                U.targets[(size_t) t]->setBounds (right.removeFromRight (rh * 1.3f * k).reduced (rh * 0.06f, rh * 0.06f).toNearestInt());
                U.targets[(size_t) t]->setTextScale (0.5f);
            }
            right.removeFromRight (rh * 0.3f * k);
            U.on->setBounds (right.removeFromRight (rh * 1.6f * k).toNearestInt());
            U.on->setTextScale (0.5f);

            // Perillas PARAM y WET en el medio.
            const auto radius = rh * 0.3f;
            const auto labelH = rh * 0.34f;
            U.param->setGeometry (radius, labelH);
            U.wet->setGeometry (radius, labelH);
            const auto half = r.getWidth() / 2.0f;
            U.param->setBounds (U.param->boundsForCentre ({ r.getX() + radius * 1.5f + 2.0f, r.getCentreY() }).toNearestInt());
            U.wet->setBounds (U.wet->boundsForCentre ({ r.getX() + half + radius * 1.5f, r.getCentreY() }).toNearestInt());
        }
    }

    void FxPanel::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();
        const auto h = b.getHeight();
        for (int u = 0; u < kNumUnits; ++u)
        {
            const auto& U = units[(size_t) u];
            const auto unitW = (b.getWidth() - h * 0.6f) / 2.0f;
            const auto area = (u == 0 ? b.withWidth (unitW) : b.withTrimmedLeft (unitW + h * 0.6f)).reduced (1.0f);
            g.setColour (panel);
            g.fillRoundedRectangle (area, h * 0.15f);
            g.setColour ((U.state.on ? play : panelEdge).withAlpha (U.state.on ? 0.7f : 1.0f));
            g.drawRoundedRectangle (area, h * 0.15f, 1.2f);

            draw::text (g, tr ("fx.unit", { { "n", juce::String (u + 1) } }), U.title, h * 0.32f,
                        U.state.on ? play : textPrimary);
            const auto beats = U.state.usesBeats ? beatsText (U.state.beatsIndex) + " " + tr ("fx.beats") : juce::String ("--");
            draw::text (g, beats, U.beatsArea, h * 0.27f, U.state.usesBeats ? cyan : textSecondary);
        }
    }
} // namespace kb::ui
