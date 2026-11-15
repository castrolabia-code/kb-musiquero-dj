#pragma once
// =============================================================================
//  PerformanceController (Fase 4): qué hace cada pad, el plato y los botones
//  SHIFT / SLIP / VINYL de cada deck. La vista (ControllerView) solo dibuja y
//  avisa; el motor solo ejecuta. Acá vive la "regla de juego":
//
//    Modo (botón)        Pads 1-4                      Con SHIFT (modo secundario)
//    HOT CUE             cues 1-4 (graba / salta)      cues 5-8
//    FX                  FRENO, SPIN, CENSOR, REVERSA  ROLL (1/8, 1/4, 1/2, 1 beat)
//    SAMPLER             samples 1-4                   samples 5-8
//    LOOP                auto-loop 1, 2, 4, 8 beats    manual: IN, OUT, ½, ×2
//
//  SHIFT + pad de hot cue = borrarlo. SHIFT + pad de sampler = detenerlo.
//  Click derecho en un pad: menú (cargar sample, volumen, modo, borrar cue...).
//
//  Plato: en modo VINYL tocarlo hace scratch; si no, girarlo es pitch bend
//  (empujar = más rápido, frenar = más lento). La rueda del mouse también
//  hace pitch bend.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include "app/AppSettings.h"
#include "app/PlayerController.h"
#include "engine/TrackLoader.h"
#include "ui/ControllerView.h"

#include <array>

namespace kb::app
{
    class PerformanceController final
    {
    public:
        enum class PadMode { hotCue, hotCue2, fx, roll, sampler, sampler2, loop, loopManual };
        static constexpr int kSlotsPerDeck = 8;

        PerformanceController (PlayerController& player, AppSettings& settings);
        ~PerformanceController();

        // --- Desde la vista ------------------------------------------------------------------
        void modeButton (int deck, int group);
        void pad (int deck, int padIndex, bool down);
        void padMenu (int deck, int padIndex, juce::Component* target);
        void shift (int deck, bool down);
        void toggleSlip (int deck);
        void toggleVinyl (int deck);
        void jogTouch (int deck, bool touching);
        void jogMove (int deck, double rotations);
        void jogWheel (int deck, float delta);

        /** Archivo soltado sobre un pad: si el deck está en modo SAMPLER lo carga ahí. */
        bool dropOnPad (int deck, int padIndex, const juce::File& file);

        // --- Sampler ---------------------------------------------------------------------------
        void loadSample (int slot, const juce::File& file);
        void clearSample (int slot);
        juce::String getSampleName (int slot) const;

        // --- Estado para la vista -----------------------------------------------------------
        ui::ControllerView::PadView makePadView (int deck) const;
        bool isShift (int deck) const;
        bool isVinyl (int deck) const { return vinyl[(size_t) deck]; }
        bool isSlip (int deck) const  { return slip[(size_t) deck]; }
        PadMode getMode (int deck) const { return mode[(size_t) deck]; }

        static juce::Colour hotCueColour (int index);

    private:
        int  slotFor (int deck, int padIndex) const;
        void chooseSample (int slot);
        void saveSamplers();
        void restoreSamplers();

        PlayerController& player;
        engine::AudioEngine& engine;
        AppSettings& settings;
        engine::TrackLoader sampleLoader { engine::Sampler::kNumSlots };

        std::array<PadMode, 2> mode { PadMode::hotCue, PadMode::hotCue };
        std::array<bool, 2> shiftHeld { false, false }, vinyl { true, true }, slip { false, false };
        std::array<int, 2> rollPad { -1, -1 };
        std::array<bool, 2> touching { false, false };

        std::array<juce::File, engine::Sampler::kNumSlots> sampleFiles;
        std::unique_ptr<juce::FileChooser> chooser;

        JUCE_DECLARE_WEAK_REFERENCEABLE (PerformanceController)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformanceController)
    };
} // namespace kb::app
