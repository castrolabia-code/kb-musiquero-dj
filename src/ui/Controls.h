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

#include <cmath>
#include <functional>

namespace kb::ui
{
    /** Símbolos que pueden llevar los botones (además del texto). */
    enum class ButtonIcon { none, play, pause, cue, sync, headphones };

    class GlowButton final : public juce::Button,
                             private juce::Timer
    {
    public:
        GlowButton (juce::String labelKey, juce::Colour colour, bool round, ButtonIcon icon = ButtonIcon::none);

        /** Ayuda al pasar el mouse (clave de traducción). */
        void setTooltipKey (const juce::String& key) { tooltipKey = key; }
        juce::String getTooltip() override;

        /** Tamaño de letra relativo al alto del botón (por defecto 0.2 redondo / 0.36 rectangular). */
        void setTextScale (float s) { if (! juce::exactlyEqual (s, textScale)) { textScale = s; repaint(); } }

        void setLabelKey (const juce::String& key) { if (key != labelKey || labelText.isNotEmpty()) { labelKey = key; labelText = {}; repaint(); } }
        /** Texto fijo (no se traduce): nombres de samples, números de cue... */
        void setLabelText (const juce::String& newText) { if (newText != labelText || labelKey.isNotEmpty()) { labelText = newText; labelKey = {}; repaint(); } }
        void setColour (juce::Colour c) { if (c != colour) { colour = c; repaint(); } }
        /** Barra de avance abajo del botón (0..1); < 0 = sin barra. */
        void setProgress (float p) { if (std::abs (p - progress) > 0.004f) { progress = p; repaint(); } }
        /** Pad de performance: cuadrado negro con borde de neón grueso (como la referencia). */
        void setPadStyle (bool p) { padStyle = p; repaint(); }
        void setIcon (ButtonIcon i) { icon = i; repaint(); }
        /** Click derecho (en vez de apretar el botón). */
        std::function<void()> onRightClick;
        void setLit (bool shouldBeLit);
        bool isLit() const noexcept { return lit; }

        /** Llamados al apretar / soltar con el mouse (o al tocar), y con doble click. */
        std::function<void()> onPress, onRelease, onDoubleClick;

    private:
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
        void buttonStateChanged() override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::String label() const;

        juce::String labelKey, labelText, tooltipKey;
        float progress = -1.0f;
        bool padStyle = false;
        float hover = 0.0f;   // 0..1, animado (el brillo entra y sale suave)
        void timerCallback() override;
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
        /** Fader de TEMPO: pista negra fina y tapa con línea naranja (la regla se dibuja en el fondo). */
        void setThinStyle (bool t) { thin = t; repaint(); }
        void paint (juce::Graphics& g) override;

        void setTooltipKey (const juce::String& key) { tooltipKey = key; }
        juce::String getTooltip() override;

    private:
        bool vertical;
        bool thin = false;
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
        /** Dónde va el nombre: 0 debajo (por defecto), -1 a la izquierda, +1 a la derecha. */
        void setLabelSide (int side) { labelSide = side; repaint(); }
        /** Nombre en negro (perilla sobre el chasis amarillo). */
        void setOnChassis (bool c) { onChassis = c; repaint(); }
        /** Etiqueta que depende del valor (p. ej. "MIX 10 s"). Si no se define, se usa la clave. */
        void setLabelFormatter (std::function<juce::String (double)> f) { labelFormatter = std::move (f); repaint(); }
        /** Tamaño que necesita el componente para la geometría dada. */
        juce::Rectangle<float> boundsForCentre (juce::Point<float> centre) const;

        void paint (juce::Graphics& g) override;

    private:
        juce::String labelKey, tooltipKey;
        std::function<juce::String (double)> labelFormatter;
        float radius = 12.0f, labelHeight = 8.0f;
        bool bipolar = false;
        int labelSide = 0;
        bool onChassis = false;
    };
} // namespace kb::ui
