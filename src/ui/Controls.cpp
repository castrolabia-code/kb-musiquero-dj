#include "ui/Controls.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

namespace kb::ui
{
    // =========================================================================
    //  GlowButton
    // =========================================================================
    GlowButton::GlowButton (juce::String key, juce::Colour c, bool r)
        : juce::Button (key), labelKey (std::move (key)), colour (c), round (r)
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

    void GlowButton::buttonStateChanged()
    {
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

        const auto c = highlighted && ! lit ? colour.brighter (0.25f) : colour;
        draw::button (g, r, c, tr (labelKey), lit || down, round, r.getHeight() * (round ? 0.2f : 0.36f));
    }

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
        // La tapa sobresale de la ranura: se reserva medio alto de tapa en cada punta.
        auto r = getLocalBounds().toFloat();
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
        const auto w = juce::jmax (radius * 5.0f, arc * 2.0f + 4.0f);
        const auto top = arc + 3.0f;
        const auto h = top + arc + labelHeight * 1.8f;
        return { centre.x - w * 0.5f, centre.y - top, w, h };
    }

    void KbKnob::paint (juce::Graphics& g)
    {
        const auto arc = radius * 1.32f;
        const juce::Point<float> centre (getWidth() * 0.5f, arc + 3.0f);
        draw::knob (g, centre, radius, (float) valueToProportionOfLength (getValue()), tr (labelKey), labelHeight);
    }
} // namespace kb::ui
