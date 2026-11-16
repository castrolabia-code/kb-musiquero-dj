#include "ui/KbLookAndFeel.h"

#include <atomic>

#include "ui/Palette.h"

#include <KbBinaryData.h>

namespace kb::ui
{
    KbLookAndFeel::KbLookAndFeel()
    {
        using namespace palette;

        auto scheme = juce::LookAndFeel_V4::getDarkColourScheme();
        scheme.setUIColour (ColourScheme::windowBackground, panelDark);
        scheme.setUIColour (ColourScheme::widgetBackground, panelLight);
        scheme.setUIColour (ColourScheme::menuBackground,   panel);
        scheme.setUIColour (ColourScheme::outline,          panelEdge);
        scheme.setUIColour (ColourScheme::defaultText,      textPrimary);
        scheme.setUIColour (ColourScheme::defaultFill,      cyan.withAlpha (0.55f));
        scheme.setUIColour (ColourScheme::highlightedText,  panelDark);
        scheme.setUIColour (ColourScheme::highlightedFill,  cyan);
        scheme.setUIColour (ColourScheme::menuText,         textPrimary);
        setColourScheme (scheme);

        setColour (juce::ResizableWindow::backgroundColourId, panelDark);
        setColour (juce::DocumentWindow::textColourId,        textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId,  panelDark);
        setColour (juce::TabbedButtonBar::tabOutlineColourId, panelEdge);
        setColour (juce::TabbedButtonBar::frontOutlineColourId, cyan);
        setColour (juce::TabbedComponent::backgroundColourId, panelDark);
        setColour (juce::TabbedComponent::outlineColourId,    panelEdge);
        setColour (juce::ComboBox::backgroundColourId,        panelLight);
        setColour (juce::ComboBox::outlineColourId,           panelEdge);
        setColour (juce::ComboBox::arrowColourId,             orange);
        setColour (juce::TextButton::buttonColourId,          panelLight);
        setColour (juce::TextButton::buttonOnColourId,        cyan);
        setColour (juce::TextButton::textColourOnId,          panelDark);
        setColour (juce::ListBox::backgroundColourId,         panelDark);
        setColour (juce::Label::textColourId,                 textPrimary);
        setColour (juce::ToggleButton::tickColourId,          cyan);
        setColour (juce::TooltipWindow::backgroundColourId,   panelLight.brighter (0.1f));
        setColour (juce::TooltipWindow::textColourId,         textPrimary);
        setColour (juce::TooltipWindow::outlineColourId,      cyan.withAlpha (0.6f));
    }

    // =========================================================================
    //  Tipografías embebidas
    // =========================================================================
    namespace
    {
        struct Typefaces
        {
            juce::Typeface::Ptr orbitronBlack, orbitronBold, rajdhaniSemiBold, rajdhaniBold;

            static Typefaces& get()
            {
                static Typefaces t;
                if (t.rajdhaniBold == nullptr)
                {
                    t.orbitronBlack    = juce::Typeface::createSystemTypefaceFor (KbBinaryData::OrbitronBlack_ttf,    (size_t) KbBinaryData::OrbitronBlack_ttfSize);
                    t.orbitronBold     = juce::Typeface::createSystemTypefaceFor (KbBinaryData::OrbitronBold_ttf,     (size_t) KbBinaryData::OrbitronBold_ttfSize);
                    t.rajdhaniSemiBold = juce::Typeface::createSystemTypefaceFor (KbBinaryData::RajdhaniSemiBold_ttf, (size_t) KbBinaryData::RajdhaniSemiBold_ttfSize);
                    t.rajdhaniBold     = juce::Typeface::createSystemTypefaceFor (KbBinaryData::RajdhaniBold_ttf,     (size_t) KbBinaryData::RajdhaniBold_ttfSize);
                }
                return t;
            }
        };

        constexpr float kRajdhaniScale = 1.3f;
    }

    void KbLookAndFeel::releaseFonts()
    {
        auto& t = Typefaces::get();
        t.orbitronBlack = t.orbitronBold = t.rajdhaniSemiBold = t.rajdhaniBold = nullptr;
    }

    juce::Font KbLookAndFeel::labelFont (float height, bool bold)
    {
        auto& t = Typefaces::get();
        return juce::Font (juce::FontOptions (bold ? t.rajdhaniBold : t.rajdhaniSemiBold).withHeight (height * kRajdhaniScale));
    }

    juce::Font KbLookAndFeel::displayFont (float height, bool black)
    {
        auto& t = Typefaces::get();
        return juce::Font (juce::FontOptions (black ? t.orbitronBlack : t.orbitronBold).withHeight (height));
    }

    juce::Typeface::Ptr KbLookAndFeel::getTypefaceForFont (const juce::Font& font)
    {
        // La fuente "por defecto" de JUCE pasa a ser Rajdhani en toda la app.
        if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        {
            auto& t = Typefaces::get();
            return font.isBold() ? t.rajdhaniBold : t.rajdhaniSemiBold;
        }
        return LookAndFeel_V4::getTypefaceForFont (font);
    }

    static std::atomic<float> gUiScale { 1.0f };
    void  KbLookAndFeel::setUiScale (float s) noexcept { gUiScale.store (juce::jlimit (1.0f, 4.0f, s)); }
    float KbLookAndFeel::uiScale() noexcept            { return gUiScale.load(); }

    static juce::Font uiFont (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    static float S (float v) { return v * KbLookAndFeel::uiScale(); }

    juce::Font KbLookAndFeel::getPopupMenuFont()                                         { return uiFont (S (21.0f)); }
    juce::Font KbLookAndFeel::getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) { return uiFont (S (21.0f)); }
    int        KbLookAndFeel::getDefaultMenuBarHeight()                                  { return juce::roundToInt (S (26.0f)); }
    juce::Font KbLookAndFeel::getComboBoxFont (juce::ComboBox& b)                         { return uiFont (juce::jmin (S (21.0f), (float) b.getHeight() * 0.8f)); }
    juce::Font KbLookAndFeel::getLabelFont (juce::Label& l)                               { return uiFont (juce::jmax (S (19.0f), l.getFont().getHeight()), l.getFont().isBold()); }
    juce::Font KbLookAndFeel::getTextButtonFont (juce::TextButton&, int h)                { return uiFont (juce::jmin (S (21.0f), (float) h * 0.7f), true); }
    juce::Font KbLookAndFeel::getAlertWindowTitleFont()                                   { return uiFont (S (24.0f), true); }
    juce::Font KbLookAndFeel::getAlertWindowMessageFont()                                 { return uiFont (S (20.0f)); }
    juce::Font KbLookAndFeel::getAlertWindowFont()                                        { return uiFont (S (19.0f)); }
    juce::Font KbLookAndFeel::getTabButtonFont (juce::TabBarButton&, float h)             { return uiFont (juce::jmin (S (21.0f), h * 0.7f), true); }

    void KbLookAndFeel::drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent& header, const juce::String& name,
                                               int columnId, int width, int height, bool over, bool down, int flags)
    {
        juce::ignoreUnused (columnId);
        const auto highlight = header.findColour (juce::TableHeaderComponent::highlightColourId);
        if (down)      g.fillAll (highlight);
        else if (over) g.fillAll (highlight.withMultipliedAlpha (0.625f));

        juce::Rectangle<int> area (width, height);
        area.reduce (6, 0);
        if ((flags & (juce::TableHeaderComponent::sortedForwards | juce::TableHeaderComponent::sortedBackwards)) != 0)
        {
            juce::Path sortArrow;
            sortArrow.addTriangle (0.0f, 0.0f, 0.5f, (flags & juce::TableHeaderComponent::sortedForwards) != 0 ? -0.8f : 0.8f, 1.0f, 0.0f);
            g.setColour (palette::cyan);
            g.fillPath (sortArrow, sortArrow.getTransformToScaleToFit (area.removeFromRight (height / 2).reduced (2).toFloat(), true));
        }
        g.setColour (header.findColour (juce::TableHeaderComponent::textColourId));
        g.setFont (uiFont ((float) height * 0.66f, true));
        g.drawFittedText (name, area, juce::Justification::centredLeft, 1);
    }

    void KbLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
    {
        const auto fontSize = juce::jmin (S (20.0f), (float) button.getHeight() * 0.8f);
        const auto tickWidth = fontSize * 1.1f;
        drawTickBox (g, button, 4.0f, ((float) button.getHeight() - tickWidth) * 0.5f, tickWidth, tickWidth,
                     button.getToggleState(), button.isEnabled(), highlighted, down);
        g.setColour (button.findColour (juce::ToggleButton::textColourId));
        g.setFont (uiFont (fontSize));
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().withTrimmedLeft (juce::roundToInt (tickWidth) + 10).withTrimmedRight (2),
                          juce::Justification::centredLeft, 2);
    }

    juce::Rectangle<int> KbLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
    {
        juce::TextLayout tl;
        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, uiFont (S (19.0f)), palette::textPrimary);
        tl.createLayoutWithBalancedLineLengths (s, S (460.0f));
        const auto w = (int) (tl.getWidth() + 24.0f);
        const auto h = (int) (tl.getHeight() + 16.0f);
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                     pos.y > parent.getCentreY() ? pos.y - (h + 6)  : pos.y + 6, w, h)
                   .constrainedWithin (parent);
    }

    void KbLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        const auto b = juce::Rectangle<int> (width, height).toFloat();
        g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (findColour (juce::TooltipWindow::outlineColourId));
        g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.2f);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, uiFont (S (19.0f)), findColour (juce::TooltipWindow::textColourId));
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, (float) width - 24.0f);
        tl.draw (g, b.reduced (12.0f, 8.0f));
    }
} // namespace kb::ui
