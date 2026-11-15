#pragma once
// =============================================================================
//  KbLookAndFeel: estilo de toda la app.
//
//  Tipografías embebidas (SIL Open Font License 1.1, ver assets/fonts/):
//    * Orbitron (Black/Bold): logo y títulos, look futurista.
//    * Rajdhani (SemiBold/Bold): todo el resto; condensada y muy legible, así
//      las letras pueden ser grandes sin desbordar.
//  Todos los textos de JUCE (menús, tablas, combos, diálogos, tooltips) usan
//  Rajdhani y tamaños grandes.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

namespace kb::ui
{
    class KbLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        KbLookAndFeel();

        /** Fuente de etiquetas (Rajdhani). 'height' es el alto de diseño; la fuente
            se agranda un 30 % porque Rajdhani es más chica a igual cuerpo. */
        static juce::Font labelFont (float height, bool bold = true);
        /** Fuente de títulos y logo (Orbitron). */
        static juce::Font displayFont (float height, bool black = true);
        /** Libera las tipografías (llamar al cerrar la app). */
        static void releaseFonts();

        // --- Tipografía para todos los componentes de JUCE ---------------------------------
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;
        juce::Font getPopupMenuFont() override;
        juce::Font getMenuBarFont (juce::MenuBarComponent&, int itemIndex, const juce::String& itemText) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        juce::Font getAlertWindowTitleFont() override;
        juce::Font getAlertWindowMessageFont() override;
        juce::Font getAlertWindowFont() override;
        juce::Font getTabButtonFont (juce::TabBarButton&, float height) override;
        void drawTableHeaderColumn (juce::Graphics&, juce::TableHeaderComponent&, const juce::String& columnName,
                                    int columnId, int width, int height, bool isMouseOver, bool isMouseDown,
                                    int columnFlags) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    };
} // namespace kb::ui
