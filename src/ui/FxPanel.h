#pragma once
// =============================================================================
//  FxPanel (Fase 5): barra de las 2 unidades de efectos, en la pantalla negra
//  entre las waveforms y la biblioteca.
//
//    [FX 1] [ECHO ▼] [◀ 1 BEAT ▶] (PARAM) (WET) [ON]  [1][2][M]   |   [FX 2] ...
//
//  La vista no conoce al motor: avisa por callbacks y recibe el estado.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Controls.h"

#include <array>
#include <functional>
#include <memory>

namespace kb::ui
{
    class FxPanel final : public juce::Component
    {
    public:
        static constexpr int kNumUnits = 2;

        FxPanel();

        struct UnitState
        {
            int   type = 0;          // fx::Type
            bool  on = false;
            float wet = 0.5f, param = 0.5f;
            int   beatsIndex = 3;
            int   target = 2;        // 0 deck 1, 1 deck 2, 2 master
            bool  usesBeats = true;
        };
        void setUnit (int unit, const UnitState& state);

        std::function<void (int unit, int type)>    onType;
        std::function<void (int unit, bool on)>     onOn;
        std::function<void (int unit, float wet)>   onWet;
        std::function<void (int unit, float param)> onParam;
        std::function<void (int unit, int index)>   onBeats;
        std::function<void (int unit, int target)>  onTarget;

        /** Vuelve a cargar los nombres (cambio de idioma). */
        void refreshTexts();

        void paint (juce::Graphics& g) override;
        void resized() override;

        /** Nombres de los efectos y de su parámetro (claves de traducción). */
        static const char* effectKey (int type);
        static const char* paramKey (int type);
        static juce::String beatsText (int index);

    private:
        struct Unit
        {
            juce::ComboBox type;
            std::unique_ptr<GlowButton> beatsDown, beatsUp, on;
            std::array<std::unique_ptr<GlowButton>, 3> targets;
            std::unique_ptr<KbKnob> param, wet;
            juce::Rectangle<float> title, beatsArea;
            UnitState state;
        };
        std::array<Unit, kNumUnits> units;
        std::array<bool, kNumUnits> formatterSet {};
    };
} // namespace kb::ui
