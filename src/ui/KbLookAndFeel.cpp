#include "ui/KbLookAndFeel.h"

#include "ui/Palette.h"

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

    juce::Font KbLookAndFeel::labelFont (float height, bool bold)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
} // namespace kb::ui
