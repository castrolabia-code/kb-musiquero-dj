#pragma once
// =============================================================================
//  Controles interactivos del skin (vectoriales, con estados hover/pressed).
//
//    GlowButton : botón redondo o rectangular con glow (PLAY, CUE...).
//                 Avisa al apretar y al soltar (CUE necesita ambos).
//    KbFader    : fader vertical u horizontal (canal, crossfader).
//    KbKnob     : perilla con arco de valor naranja.
//
//  Los textos se pasan como CLAVES de traducción y se traducen al pintar, así
//  el cambio de idioma solo requiere repintar. Lo mismo con la ayuda que
//  aparece al pasar el mouse (tooltip): setTooltipKey ("tip.xxx").
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace kb::ui
{
    /** Símbolos que pueden llevar los botones (además del texto). */
    enum class ButtonIcon { none, play, pause, cue, sync };

    class GlowButton final : public juce::Button
    {
    public:
        GlowButton (juce::String labelKey, juce::Colour colour, bool round, ButtonIcon icon = ButtonIcon::none);

        /** Ayuda al pasar el mouse (clave de traducción). */
        void setTooltipKey (const juce::String& key) { tooltipKey = key; }
        juce::String getTooltip() override;

        /** Tamaño de letra relativo al alto del botón (por defecto 0.2 redondo / 0.36 rectangular). */
        void setTextScale (float s) { textScale = s; repaint(); }

        void setLabelKey (const juce::String& key) { labelKey = key; repaint(); }
        void setLit (bool shouldBeLit);
        bool isLit() const noexcept { return lit; }

        /** Llamados al apretar / soltar con el mouse (o al tocar). */
        std::function<void()> onPress, onRelease;

    private:
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
        void buttonStateChanged() override;

        juce::String labelKey, tooltipKey;
        juce::Colour colour;
        bool round;
        ButtonIcon icon;
        float textScale = -1.0f;
        bool lit = false;
        bool wasDown = false;
    };

    class KbFader final : public juce::Slider
    {
    public:
        explicit KbFader (bool vertical);
        void paint (juce::Graphics& g) override;

        void setTooltipKey (const juce::String& key) { tooltipKey = key; }
        juce::String getTooltip() override;

    private:
        bool vertical;
        juce::String tooltipKey;
    };

    class KbKnob final : public juce::Slider
    {
    public:
        explicit KbKnob (juce::String labelKey);

        /** Radio de la perilla y alto de la etiqueta, en píxeles. */
        void setGeometry (float radius, float labelHeight);
        void setTooltipKey (const juce::String& key) { tooltipKey = key; }
        juce::String getTooltip() override;

        /** Arco desde el centro (EQ, GAIN, FILTER). */
        void setBipolar (bool b) { bipolar = b; repaint(); }
        /** Tamaño que necesita el componente para la geometría dada. */
        juce::Rectangle<float> boundsForCentre (juce::Point<float> centre) const;

        void paint (juce::Graphics& g) override;

    private:
        juce::String labelKey, tooltipKey;
        float radius = 12.0f, labelHeight = 8.0f;
        bool bipolar = false;
    };
} // namespace kb::ui
