#include "ui/Controls.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

namespace kb::ui
{
    // =========================================================================
    //  GlowButton
    // =========================================================================
    GlowButton::GlowButton (juce::String key, juce::Colour c, bool r, ButtonIcon i)
        : juce::Button (key), labelKey (std::move (key)), colour (c), round (r), icon (i)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);
    }

    void GlowButton::setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }

    void GlowButton::timerCallback()
    {
        const auto target = isOver() ? 1.0f : 0.0f;
        hover += (target - hover) * 0.35f;
        if (std::abs (target - hover) < 0.02f)
        {
            hover = target;
            stopTimer();
        }
        repaint();
    }

    void GlowButton::buttonStateChanged()
    {
        if (! juce::exactlyEqual (isOver() ? 1.0f : 0.0f, hover) && ! isTimerRunning())
            startTimerHz (60);
        const auto down = isDown();
        if (down && ! wasDown && onPress)
            onPress();
        else if (! down && wasDown && onRelease)
            onRelease();
        wasDown = down;
    }

    void GlowButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
    {
        auto r = getLocalBounds().toFloat();
        if (round)
            r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));

        // Margen para el glow; al apretar el botón "se hunde" un poco.
        r = r.reduced (r.getHeight() * 0.08f);
        if (down)
            r = r.reduced (r.getHeight() * 0.03f);

        juce::ignoreUnused (highlighted);
        const auto c = lit ? colour : colour.brighter (0.3f * hover);
        const auto scale = textScale > 0.0f ? textScale : (round ? 0.25f : 0.46f);

        if (padStyle)
        {
            const auto h = r.getHeight();
            juce::Path shape;
            shape.addRoundedRectangle (r, h * 0.07f);
            const auto on = lit || down;
            juce::DropShadow (c.withAlpha (on ? 0.9f : 0.55f), juce::roundToInt (h * (on ? 0.32f : 0.18f)), {}).drawForPath (g, shape);
            g.setGradientFill (juce::ColourGradient (on ? c.withAlpha (0.9f) : juce::Colour (0xff161a1f), r.getX(), r.getY(),
                                                     on ? c.darker (0.45f) : juce::Colour (0xff07080a), r.getRight(), r.getBottom(), false));
            g.fillPath (shape);
            g.setColour (on ? c.brighter (0.7f) : c);
            g.strokePath (shape, juce::PathStrokeType (juce::jmax (2.0f, h * 0.055f)));
            const auto padText = label();
            const auto ink = on ? palette::panelDark : c;
            if (padText.length() <= 2)   // números (cues, beats): arriba a la izquierda, como la referencia
                draw::text (g, padText, r.reduced (h * 0.1f, h * 0.06f).removeFromTop (h * 0.34f), h * scale * 0.62f, ink,
                            juce::Justification::centredLeft);
            else
                draw::text (g, padText, r.reduced (h * 0.08f, 0.0f), h * scale, ink);
            if (progress >= 0.0f)
            {
                const auto bar = r.reduced (h * 0.12f, 0.0f).removeFromBottom (h * 0.08f).translated (0.0f, -h * 0.1f);
                g.setColour (palette::panelDark.withAlpha (0.6f));
                g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
                g.setColour (on ? palette::panelDark : c);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, progress)), bar.getHeight() * 0.5f);
            }
            return;
        }

        if (icon == ButtonIcon::none)
        {
            draw::button (g, r, c, label(), lit || down, round, r.getHeight() * scale);
            if (progress >= 0.0f)
            {
                const auto bar = r.reduced (r.getWidth() * 0.12f, 0.0f).removeFromBottom (r.getHeight() * 0.12f).translated (0.0f, -r.getHeight() * 0.08f);
                g.setColour (palette::panelDark.withAlpha (0.6f));
                g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
                g.setColour (lit ? palette::panelDark : c);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, progress)), bar.getHeight() * 0.5f);
            }
            return;
        }

        if (! round)
        {
            // Rectangular: símbolo a la izquierda y texto a la derecha (o solo el símbolo).
            draw::button (g, r, c, {}, lit || down, false, r.getHeight() * scale);
            const auto ink = (lit || down) ? palette::panelDark : c;
            const auto caption = label();
            auto content = r.reduced (r.getHeight() * 0.2f);
            if (caption.isEmpty())
            {
                draw::icon (g, (int) icon, content.withSizeKeepingCentre (content.getHeight() * 0.9f, content.getHeight() * 0.9f), ink);
                return;
            }
            const auto iconSize = content.getHeight() * 0.62f;
            const auto textW = juce::jmin (content.getWidth() - iconSize * 1.3f, content.getWidth() * 0.66f);
            const auto total = iconSize * 1.3f + textW;
            auto row = content.withSizeKeepingCentre (total, content.getHeight());
            draw::icon (g, (int) icon, row.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize), ink);
            row.removeFromLeft (iconSize * 0.3f);
            draw::text (g, caption, row, r.getHeight() * scale, ink, juce::Justification::centredLeft);
            return;
        }

        // Símbolo arriba y texto chico abajo.
        draw::button (g, r, c, {}, lit || down, round, r.getHeight() * scale);
        const auto ink = (lit || down) ? palette::panelDark : c;
        const auto iconArea = r.withSizeKeepingCentre (r.getWidth() * 0.4f, r.getHeight() * 0.34f).translated (0.0f, -r.getHeight() * 0.12f);
        draw::icon (g, (int) icon, iconArea, ink);
        draw::text (g, label(), r.withTrimmedTop (r.getHeight() * 0.56f).withTrimmedBottom (r.getHeight() * 0.1f),
                    r.getHeight() * 0.19f, ink);
    }

    juce::String GlowButton::label() const
    {
        return labelKey.isNotEmpty() ? tr (labelKey) : labelText;
    }

    void GlowButton::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu() && onRightClick)
        {
            onRightClick();
            return;
        }
        juce::Button::mouseDown (e);
    }

    void GlowButton::mouseDoubleClick (const juce::MouseEvent&)
    {
        if (onDoubleClick)
            onDoubleClick();
    }

    juce::String GlowButton::getTooltip()  { return tooltipKey.isNotEmpty() ? tr (tooltipKey) : juce::String(); }
    juce::String KbFader::getTooltip()     { return tooltipKey.isNotEmpty() ? tr (tooltipKey) : juce::String(); }
    juce::String KbKnob::getTooltip()      { return tooltipKey.isNotEmpty() ? tr (tooltipKey) : juce::String(); }

    // =========================================================================
    //  KbFader
    // =========================================================================
    KbFader::KbFader (bool v)
        : juce::Slider (v ? juce::Slider::LinearVertical : juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
          vertical (v)
    {
        setRange (0.0, 1.0);
        // Como un fader real: se arrastra la tapa; un click no la hace saltar.
        setSliderSnapsToMousePosition (false);
        setVelocityBasedMode (false);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void KbFader::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        if (thin && vertical)
        {
            const auto capH = r.getWidth() * 0.38f;
            r = r.reduced (0.0f, capH * 0.5f + 1.0f);
            const auto track = r.withSizeKeepingCentre (juce::jmax (3.0f, r.getWidth() * 0.09f), r.getHeight());
            g.setColour (juce::Colour (0xff050505));
            g.fillRoundedRectangle (track, track.getWidth() * 0.5f);
            const auto y = r.getBottom() - r.getHeight() * (float) valueToProportionOfLength (getValue());
            const auto cap = juce::Rectangle<float> (r.getWidth() * 0.95f, capH).withCentre ({ r.getCentreX(), y });
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (cap.translated (2.0f, 3.0f), capH * 0.18f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4e55), 0.0f, cap.getY(), juce::Colour (0xff121315), 0.0f, cap.getBottom(), false));
            g.fillRoundedRectangle (cap, capH * 0.18f);
            const auto line = cap.withSizeKeepingCentre (cap.getWidth() * 0.86f, juce::jmax (2.0f, capH * 0.12f));
            g.setColour (palette::orange.withAlpha (0.4f));
            g.fillRoundedRectangle (line.expanded (0.0f, line.getHeight()), line.getHeight());
            g.setColour (palette::orange);
            g.fillRoundedRectangle (line, line.getHeight() * 0.5f);
            return;
        }
        // La tapa sobresale de la ranura: se reserva medio alto de tapa en cada punta.
        const auto capLong  = (vertical ? r.getWidth() : r.getHeight()) * 0.9f;
        const auto capShort = capLong * 0.45f;
        r = vertical ? r.reduced (0.0f, capShort * 0.5f + 2.0f) : r.reduced (capShort * 0.5f + 2.0f, 0.0f);

        draw::fader (g, r, vertical, (float) valueToProportionOfLength (getValue()));
    }

    // =========================================================================
    //  KbKnob
    // =========================================================================
    KbKnob::KbKnob (juce::String key)
        : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox),
          labelKey (std::move (key))
    {
        setRange (0.0, 1.0);
        setMouseDragSensitivity (180);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void KbKnob::setGeometry (float r, float labelH)
    {
        radius = r;
        labelHeight = labelH;
        repaint();
    }

    juce::Rectangle<float> KbKnob::boundsForCentre (juce::Point<float> centre) const
    {
        const auto arc = radius * 1.32f;
        if (labelSide != 0)
        {
            // Perilla a un costado y el nombre al lado (la perilla queda en 'centre').
            const auto labelW = labelHeight * 4.2f;
            const auto w = arc * 2.0f + 6.0f + labelW;
            const auto h = arc * 2.0f + 6.0f;
            const auto x = labelSide < 0 ? centre.x - arc - 3.0f - labelW : centre.x - arc - 3.0f;
            return { x, centre.y - h * 0.5f, w, h };
        }
        const auto w = juce::jmax (radius * 5.0f, arc * 2.0f + 4.0f);
        const auto top = arc + 3.0f;
        const auto h = top + arc + labelHeight * 1.8f;
        return { centre.x - w * 0.5f, centre.y - top, w, h };
    }

    void KbKnob::paint (juce::Graphics& g)
    {
        const auto arc = radius * 1.32f;
        const auto label = labelFormatter ? labelFormatter (getValue()) : tr (labelKey);
        if (labelSide != 0)
        {
            const auto w = (float) getWidth(), h = (float) getHeight();
            const juce::Point<float> c (labelSide < 0 ? w - arc - 3.0f : arc + 3.0f, h * 0.5f);
            draw::knob (g, c, radius, (float) valueToProportionOfLength (getValue()), {}, labelHeight, bipolar);
            const auto labelArea = labelSide < 0 ? juce::Rectangle<float> (0.0f, 0.0f, w - arc * 2.0f - 8.0f, h)
                                                 : juce::Rectangle<float> (arc * 2.0f + 8.0f, 0.0f, w - arc * 2.0f - 8.0f, h);
            draw::text (g, label, labelArea, labelHeight, palette::textSecondary,
                        labelSide < 0 ? juce::Justification::centredRight : juce::Justification::centredLeft);
            return;
        }
        const juce::Point<float> centre (getWidth() * 0.5f, arc + 3.0f);
        if (onChassis)
        {
            draw::knob (g, centre, radius, (float) valueToProportionOfLength (getValue()), {}, labelHeight, bipolar);
            draw::text (g, label, juce::Rectangle<float> ((float) getWidth(), labelHeight * 1.3f)
                                      .withCentre ({ centre.x, centre.y + arc + labelHeight * 0.85f }),
                        labelHeight, palette::textOnChassis);
            return;
        }
        draw::knob (g, centre, radius, (float) valueToProportionOfLength (getValue()), label, labelHeight, bipolar);
    }
} // namespace kb::ui
